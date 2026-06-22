#include "ui/ui_sprite_navigator.h"

#include "rom/spinball_rom.h"
#include "rom/bonus_stage_decoder.h"
#include "rom/tails_plane_decoder.h"
#include "rom/title_screen_decoder.h"
#include "ui/ui_editor.h"
#include "ui/ui_file_selector.h"

#include "imgui.h"
#include "SDL3/SDL_image.h"
#include "ui/ui_palette_viewer.h"
#include "rom/tile_layout.h"
#include "rom/tileset.h"
#include "rom/palette.h"
#include "types/rom_ptr.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <cstdio>
#include <cassert>
#include <optional>
#include <algorithm>
#include <thread>
#include <atomic>
#include <limits>
#include <string>
#include <utility>
#include <unordered_set>
#include <system_error>
#include <sstream>

namespace
{
	std::string PathToUtf8(const std::filesystem::path& path)
	{
#if defined(__cpp_lib_char8_t)
		const std::u8string utf8_path = path.generic_u8string();
		return std::string(
			reinterpret_cast<const char*>(utf8_path.data()),
			utf8_path.size()
		);
#else
		return path.generic_u8string();
#endif
	}

	std::vector<Uint8> CopyIndexedSurfacePixels(SDL_Surface* surface)
	{
		std::vector<Uint8> pixels;
		if (!surface || surface->format != SDL_PIXELFORMAT_INDEX8 ||
			surface->w <= 0 || surface->h <= 0 || !surface->pixels)
		{
			return pixels;
		}

		pixels.resize(
			static_cast<std::size_t>(surface->w) *
			static_cast<std::size_t>(surface->h)
		);
		for (int y = 0; y < surface->h; ++y)
		{
			const Uint8* source_row = static_cast<const Uint8*>(surface->pixels) +
				static_cast<std::size_t>(y) * surface->pitch;
			std::copy_n(
				source_row,
				static_cast<std::size_t>(surface->w),
				pixels.begin() + static_cast<std::size_t>(y) * surface->w
			);
		}
		return pixels;
	}

	std::vector<Uint8> RenderSpriteIndexedPixels(
		const spintool::rom::Sprite& sprite,
		const spintool::rom::Palette& palette,
		int& width,
		int& height
	)
	{
		const spintool::BoundingBox bounds = sprite.GetBoundingBox();
		width = bounds.Width();
		height = bounds.Height();
		if (width <= 0 || height <= 0)
		{
			return {};
		}

		SDLSurfaceHandle surface{
			SDL_CreateSurface(width, height, SDL_PIXELFORMAT_INDEX8)
		};
		if (!surface)
		{
			return {};
		}
		SDLPaletteHandle sdl_palette = spintool::Renderer::CreateSDLPalette(palette);
		SDL_SetSurfacePalette(surface.get(), sdl_palette.get());
		SDL_SetSurfaceColorKey(surface.get(), true, 0);
		sprite.RenderToSurface(surface.get());
		return CopyIndexedSurfacePixels(surface.get());
	}

	std::vector<Uint8> RenderSpriteIndexedPixels(
		const spintool::rom::Sprite& sprite,
		const spintool::rom::PaletteSet& palette_set,
		int& width,
		int& height
	)
	{
		const spintool::BoundingBox bounds = sprite.GetBoundingBox();
		width = bounds.Width();
		height = bounds.Height();
		if (width <= 0 || height <= 0)
		{
			return {};
		}

		SDLSurfaceHandle surface{
			SDL_CreateSurface(width, height, SDL_PIXELFORMAT_INDEX8)
		};
		if (!surface)
		{
			return {};
		}
		SDLPaletteHandle sdl_palette =
			spintool::Renderer::CreateSDLPaletteForSet(palette_set);
		if (!sdl_palette ||
			!SDL_SetSurfacePalette(surface.get(), sdl_palette.get()) ||
			!SDL_SetSurfaceColorKey(surface.get(), true, 0))
		{
			return {};
		}
		sprite.RenderToSurface(surface.get());
		return CopyIndexedSurfacePixels(surface.get());
	}

	std::vector<Uint8> ConvertSurfaceToIndexed(
		SDL_Surface* source,
		const spintool::rom::Palette& palette,
		const std::vector<Uint8>& preferred_indices,
		const int preferred_width,
		const int preferred_height
	)
	{
		std::vector<Uint8> output;
		if (!source || source->w <= 0 || source->h <= 0)
		{
			return output;
		}

		const bool has_preferred_indices =
			preferred_width == source->w && preferred_height == source->h &&
			preferred_indices.size() >=
				static_cast<std::size_t>(source->w) * source->h;

		auto select_palette_index = [&](const Uint8 red, const Uint8 green,
			const Uint8 blue, const Uint8 alpha, const std::size_t pixel_index) -> Uint8
		{
			if (alpha < 0x80U)
			{
				return 0U;
			}

			const Uint8 preferred = has_preferred_indices
				? static_cast<Uint8>(preferred_indices[pixel_index] & 0x0FU)
				: 0xFFU;
			Uint8 best_index = 0U;
			Uint32 best_distance = std::numeric_limits<Uint32>::max();
			for (Uint8 palette_index = 0; palette_index < 16U; ++palette_index)
			{
				const spintool::rom::Colour colour =
					palette.palette_swatches[palette_index].GetUnpacked();
				const int red_delta = static_cast<int>(red) - colour.r;
				const int green_delta = static_cast<int>(green) - colour.g;
				const int blue_delta = static_cast<int>(blue) - colour.b;
				const Uint32 distance = static_cast<Uint32>(
					red_delta * red_delta +
					green_delta * green_delta +
					blue_delta * blue_delta
				);
				if (distance < best_distance ||
					(distance == best_distance && palette_index == preferred))
				{
					best_distance = distance;
					best_index = palette_index;
				}
			}
			return best_index;
		};

		output.resize(
			static_cast<std::size_t>(source->w) *
			static_cast<std::size_t>(source->h),
			0U
		);

		// PNGs exported by SpinTool are indexed. Preserve their 0-15 indices
		// directly whenever the indexed colour still matches the selected ROM
		// palette. This keeps duplicate-looking palette entries distinct.
		if (source->format == SDL_PIXELFORMAT_INDEX8 && source->pixels)
		{
			SDL_Palette* source_palette = SDL_GetSurfacePalette(source);
			for (int y = 0; y < source->h; ++y)
			{
				const Uint8* row = static_cast<const Uint8*>(source->pixels) +
					static_cast<std::size_t>(y) * source->pitch;
				for (int x = 0; x < source->w; ++x)
				{
					const std::size_t pixel_index =
						static_cast<std::size_t>(y) * source->w + x;
					const Uint8 raw_index = row[x];
					if (raw_index < 16U && !source_palette)
					{
						output[pixel_index] = raw_index;
						continue;
					}
					if (raw_index < 16U && source_palette &&
						raw_index < source_palette->ncolors)
					{
						const SDL_Color source_colour = source_palette->colors[raw_index];
						const spintool::rom::Colour target_colour =
							palette.palette_swatches[raw_index].GetUnpacked();
						const bool transparent_index = raw_index == 0U;
						const bool opaque_matching_index = source_colour.a >= 0x80U &&
							source_colour.r == target_colour.r &&
							source_colour.g == target_colour.g &&
							source_colour.b == target_colour.b;
						if (transparent_index || opaque_matching_index)
						{
							output[pixel_index] = raw_index;
							continue;
						}
					}

					if (source_palette && raw_index < source_palette->ncolors)
					{
						const SDL_Color colour = source_palette->colors[raw_index];
						output[pixel_index] = select_palette_index(
							colour.r,
							colour.g,
							colour.b,
							colour.a,
							pixel_index
						);
					}
				}
			}
			return output;
		}

		SDLSurfaceHandle converted{
			SDL_ConvertSurface(source, SDL_PIXELFORMAT_RGBA32)
		};
		if (!converted)
		{
			return {};
		}

		const SDL_PixelFormatDetails* format =
			SDL_GetPixelFormatDetails(converted->format);
		if (!format)
		{
			return {};
		}
		const auto extract_channel = [](Uint32 packed, Uint32 mask, Uint8 shift) -> Uint8
		{
			return mask == 0 ? 0xFFU : static_cast<Uint8>((packed & mask) >> shift);
		};

		for (int y = 0; y < converted->h; ++y)
		{
			const Uint8* row = static_cast<const Uint8*>(converted->pixels) +
				static_cast<std::size_t>(y) * converted->pitch;
			for (int x = 0; x < converted->w; ++x)
			{
				const Uint32 packed = reinterpret_cast<const Uint32*>(row)[x];
				const Uint8 red = extract_channel(packed, format->Rmask, format->Rshift);
				const Uint8 green = extract_channel(packed, format->Gmask, format->Gshift);
				const Uint8 blue = extract_channel(packed, format->Bmask, format->Bshift);
				const Uint8 alpha = format->Amask == 0
					? 0xFFU
					: extract_channel(packed, format->Amask, format->Ashift);
				const std::size_t pixel_index =
					static_cast<std::size_t>(y) * converted->w + x;
				output[pixel_index] = select_palette_index(
					red,
					green,
					blue,
					alpha,
					pixel_index
				);
			}
		}
		return output;
	}


	std::vector<Uint8> ConvertSurfaceToTitleIndexed(
		SDL_Surface* source,
		const spintool::rom::PaletteSet& palette_set,
		const std::vector<Uint8>& palette_line_map,
		const std::vector<Uint8>& preferred_indices,
		const int preferred_width,
		const int preferred_height
	)
	{
		std::vector<Uint8> output;
		if (!source || source->w <= 0 || source->h <= 0)
		{
			return output;
		}
		for (const std::shared_ptr<spintool::rom::Palette>& line : palette_set.palette_lines)
		{
			if (!line) return output;
		}

		const std::size_t pixel_count =
			static_cast<std::size_t>(source->w) * source->h;
		const bool has_preferred =
			preferred_width == source->w && preferred_height == source->h &&
			preferred_indices.size() >= pixel_count;
		const bool has_line_map =
			preferred_width == source->w && preferred_height == source->h &&
			palette_line_map.size() >= pixel_count;

		auto palette_line_for = [&](const std::size_t pixel_index) -> Uint8
		{
			if (has_line_map)
			{
				return static_cast<Uint8>(palette_line_map[pixel_index] & 3U);
			}
			if (has_preferred && preferred_indices[pixel_index] != 0U)
			{
				return static_cast<Uint8>((preferred_indices[pixel_index] >> 4U) & 3U);
			}
			return 0U;
		};

		auto select_palette_index = [&](
			const Uint8 red,
			const Uint8 green,
			const Uint8 blue,
			const Uint8 alpha,
			const std::size_t pixel_index
		) -> Uint8
		{
			if (alpha < 0x80U) return 0U;
			const Uint8 line = palette_line_for(pixel_index);
			const spintool::rom::Palette& palette = *palette_set.palette_lines[line];
			const Uint8 preferred_local = has_preferred
				? static_cast<Uint8>(preferred_indices[pixel_index] & 0x0FU)
				: 0xFFU;
			Uint8 best_local = 1U;
			Uint32 best_distance = std::numeric_limits<Uint32>::max();
			// Local colour zero is transparent for Mega Drive sprites, so opaque
			// PNG pixels are quantised only to visible entries 1-15.
			for (Uint8 local = 1U; local < 16U; ++local)
			{
				const spintool::rom::Colour colour =
					palette.palette_swatches[local].GetUnpacked();
				const int red_delta = static_cast<int>(red) - colour.r;
				const int green_delta = static_cast<int>(green) - colour.g;
				const int blue_delta = static_cast<int>(blue) - colour.b;
				const Uint32 distance = static_cast<Uint32>(
					red_delta * red_delta +
					green_delta * green_delta +
					blue_delta * blue_delta
				);
				if (distance < best_distance ||
					(distance == best_distance && local == preferred_local))
				{
					best_distance = distance;
					best_local = local;
				}
			}
			return static_cast<Uint8>((line * 16U) + best_local);
		};

		output.assign(pixel_count, 0U);
		if (source->format == SDL_PIXELFORMAT_INDEX8 && source->pixels)
		{
			SDL_Palette* source_palette = SDL_GetSurfacePalette(source);
			for (int y = 0; y < source->h; ++y)
			{
				const Uint8* row = static_cast<const Uint8*>(source->pixels) +
					static_cast<std::size_t>(y) * source->pitch;
				for (int x = 0; x < source->w; ++x)
				{
					const std::size_t pixel_index =
						static_cast<std::size_t>(y) * source->w + x;
					const Uint8 raw_index = row[x];
					if (raw_index == 0U)
					{
						output[pixel_index] = 0U;
						continue;
					}
					const Uint8 expected_line = palette_line_for(pixel_index);
					if (!source_palette && raw_index < 64U)
					{
						output[pixel_index] = static_cast<Uint8>(
							(expected_line * 16U) + (raw_index & 0x0FU)
						);
						continue;
					}
					if (source_palette && raw_index < source_palette->ncolors)
					{
						const SDL_Color source_colour = source_palette->colors[raw_index];
						const Uint8 raw_line = static_cast<Uint8>((raw_index >> 4U) & 3U);
						const Uint8 raw_local = static_cast<Uint8>(raw_index & 0x0FU);
						if (raw_index < 64U && raw_line == expected_line && raw_local != 0U)
						{
							const spintool::rom::Colour target_colour =
								palette_set.palette_lines[expected_line]
									->palette_swatches[raw_local].GetUnpacked();
							if (source_colour.a >= 0x80U &&
								source_colour.r == target_colour.r &&
								source_colour.g == target_colour.g &&
								source_colour.b == target_colour.b)
							{
								output[pixel_index] = raw_index;
								continue;
							}
						}
						output[pixel_index] = select_palette_index(
							source_colour.r, source_colour.g, source_colour.b,
							source_colour.a, pixel_index
						);
					}
				}
			}
			return output;
		}

		SDLSurfaceHandle converted{ SDL_ConvertSurface(source, SDL_PIXELFORMAT_RGBA32) };
		if (!converted) return {};
		const SDL_PixelFormatDetails* format = SDL_GetPixelFormatDetails(converted->format);
		if (!format) return {};
		const auto extract_channel = [](Uint32 packed, Uint32 mask, Uint8 shift) -> Uint8
		{
			return mask == 0U ? 0xFFU : static_cast<Uint8>((packed & mask) >> shift);
		};
		for (int y = 0; y < converted->h; ++y)
		{
			const Uint8* row = static_cast<const Uint8*>(converted->pixels) +
				static_cast<std::size_t>(y) * converted->pitch;
			for (int x = 0; x < converted->w; ++x)
			{
				const Uint32 packed = reinterpret_cast<const Uint32*>(row)[x];
				const Uint8 red = extract_channel(packed, format->Rmask, format->Rshift);
				const Uint8 green = extract_channel(packed, format->Gmask, format->Gshift);
				const Uint8 blue = extract_channel(packed, format->Bmask, format->Bshift);
				const Uint8 alpha = format->Amask == 0U
					? 0xFFU : extract_channel(packed, format->Amask, format->Ashift);
				const std::size_t pixel_index =
					static_cast<std::size_t>(y) * converted->w + x;
				output[pixel_index] = select_palette_index(
					red, green, blue, alpha, pixel_index
				);
			}
		}
		return output;
	}


	const char* TitleCategoryName(const spintool::rom::TitleScreenCategory category)
	{
		switch (category)
		{
		case spintool::rom::TitleScreenCategory::SONIC:
			return "Sonic Frames";
		case spintool::rom::TitleScreenCategory::BUMPER_RING:
			return "Bumper / Ring";
		case spintool::rom::TitleScreenCategory::LOGO_SONIC:
			return "Logo Sonic";
		case spintool::rom::TitleScreenCategory::LOGO_THE_HEDGEHOG:
			return "Logo The Hedgehog";
		case spintool::rom::TitleScreenCategory::LOGO_SPINBALL:
			return "Logo Spinball";
		}
		return "Title Frame";
	}

	const char* TitleCategorySlug(const spintool::rom::TitleScreenCategory category)
	{
		switch (category)
		{
		case spintool::rom::TitleScreenCategory::SONIC:
			return "sonic";
		case spintool::rom::TitleScreenCategory::BUMPER_RING:
			return "bumper_ring";
		case spintool::rom::TitleScreenCategory::LOGO_SONIC:
			return "logo_sonic";
		case spintool::rom::TitleScreenCategory::LOGO_THE_HEDGEHOG:
			return "logo_the_hedgehog";
		case spintool::rom::TitleScreenCategory::LOGO_SPINBALL:
			return "logo_spinball";
		}
		return "title";
	}

	void UpdateMegaDriveChecksum(spintool::rom::SpinballROM& rom)
	{
		if (rom.m_buffer.size() < 0x190U)
		{
			return;
		}

		Uint32 checksum = 0;
		for (std::size_t offset = 0x200U; offset < rom.m_buffer.size(); offset += 2U)
		{
			Uint16 word = static_cast<Uint16>(rom.m_buffer[offset]) << 8U;
			if (offset + 1U < rom.m_buffer.size())
			{
				word = static_cast<Uint16>(word | rom.m_buffer[offset + 1U]);
			}
			checksum = (checksum + word) & 0xFFFFU;
		}

		rom.m_buffer[0x18EU] = static_cast<Uint8>((checksum >> 8U) & 0xFFU);
		rom.m_buffer[0x18FU] = static_cast<Uint8>(checksum & 0xFFU);
	}
}

namespace spintool
{
	void EditorSpriteNavigator::LoadBonusStageImages()
	{
		m_result_display_mode = ResultDisplayMode::BONUS_OBJECT_FRAMES;
		m_bonus_stage_images.clear();
		m_bonus_stage_warnings.clear();
		m_bonus_stage_status.clear();
		m_selected_bonus_image = -1;

		const rom::BonusStageDecodeResult decode_result =
			rom::BonusStageDecoder::DecodeObjectFrames(
				m_owning_ui.GetROM(),
				static_cast<rom::BonusStageId>(m_selected_bonus_stage)
			);

		m_bonus_stage_warnings = decode_result.warnings;
		if (!decode_result.error.empty())
		{
			m_bonus_stage_status = "Error: " + decode_result.error;
			return;
		}

		std::unordered_set<Uint64> seen_images;
		for (const rom::BonusStageObjectFrames& decoded_object : decode_result.objects)
		{
			const Uint16 visual_tile_attributes = static_cast<Uint16>(
				decoded_object.base_tile_attributes & 0x7FFFU
			);
			for (const rom::BonusStageAnimationState& decoded_state : decoded_object.states)
			{
				for (const rom::BonusStageFrame& decoded_frame : decoded_state.frames)
				{
					if (!decoded_frame.sprite)
					{
						continue;
					}
					const Uint64 visual_key =
						(static_cast<Uint64>(decoded_frame.mapping_offset) << 16U) |
						static_cast<Uint64>(visual_tile_attributes);
					if (!seen_images.insert(visual_key).second)
					{
						continue;
					}

					std::shared_ptr<const rom::Sprite> frame_sprite = decoded_frame.sprite;
					BonusStageImagePreview image_preview;
					image_preview.mapping_offset = decoded_frame.mapping_offset;
					image_preview.visual_tile_attributes = visual_tile_attributes;
					image_preview.texture = std::make_shared<UISpriteTexture>(frame_sprite);
					m_bonus_stage_images.emplace_back(std::move(image_preview));
				}
			}
		}

		if (m_bonus_stage_images.empty())
		{
			m_bonus_stage_status = "No Bonus Stage image could be decoded.";
		}
	}

	void EditorSpriteNavigator::ImportBonusStageImage(
		const std::filesystem::path& path,
		std::size_t image_index
	)
	{
		if (image_index >= m_bonus_stage_images.size())
		{
			m_bonus_stage_status = "The selected Bonus Stage image no longer exists.";
			return;
		}

		const std::string path_utf8 = PathToUtf8(path);
		SDLSurfaceHandle loaded_image{ IMG_Load(path_utf8.c_str()) };
		if (!loaded_image)
		{
			m_bonus_stage_status = "Could not load PNG: " + path_utf8;
			return;
		}

		const auto& palettes = m_owning_ui.GetPalettes();
		if (palettes.empty())
		{
			m_bonus_stage_status = "No palette is loaded.";
			return;
		}
		m_chosen_palette = std::clamp(
			m_chosen_palette,
			0,
			static_cast<int>(palettes.size()) - 1
		);
		const BonusStageImagePreview target = m_bonus_stage_images[image_index];
		if (!target.texture || !target.texture->sprite)
		{
			m_bonus_stage_status = "The selected Bonus Stage image has no sprite data.";
			return;
		}
		int preferred_width = 0;
		int preferred_height = 0;
		const std::vector<Uint8> preferred_indices = RenderSpriteIndexedPixels(
			*target.texture->sprite,
			*palettes[static_cast<std::size_t>(m_chosen_palette)],
			preferred_width,
			preferred_height
		);
		const std::vector<Uint8> indexed_pixels = ConvertSurfaceToIndexed(
			loaded_image.get(),
			*palettes[static_cast<std::size_t>(m_chosen_palette)],
			preferred_indices,
			preferred_width,
			preferred_height
		);
		if (indexed_pixels.empty())
		{
			m_bonus_stage_status = "The PNG could not be converted to indexed pixels.";
			return;
		}

		rom::SpinballROM& rom = m_owning_ui.GetROM();
		const std::filesystem::path saved_rom_path = rom.m_filepath;
		if (saved_rom_path.empty())
		{
			m_bonus_stage_status = "The working ROM has no file path and cannot be saved.";
			return;
		}
		const std::vector<Uint8> original_rom_buffer = rom.m_buffer;

		const std::filesystem::path reference_rom_path =
			m_owning_ui.GetReferenceROMPath();
		if (reference_rom_path.empty())
		{
			m_bonus_stage_status =
				"No reference ROM is associated with the current working ROM.";
			return;
		}

		rom::SpinballROM reference_rom;
		if (!reference_rom.LoadROMFromPath(reference_rom_path))
		{
			m_bonus_stage_status = "Could not load reference ROM: " +
				PathToUtf8(reference_rom_path);
			return;
		}

		const rom::BonusStageImportResult import_result =
			rom::BonusStageDecoder::ImportIndexedImage(
				rom,
				reference_rom,
				static_cast<rom::BonusStageId>(m_selected_bonus_stage),
				target.mapping_offset,
				target.visual_tile_attributes,
				indexed_pixels,
				loaded_image->w,
				loaded_image->h
			);
		if (!import_result.success)
		{
			m_bonus_stage_status = "Import failed: " + import_result.message;
			return;
		}
		if (!import_result.changed)
		{
			m_bonus_stage_status = import_result.message;
			return;
		}

		std::filesystem::path backup_path = saved_rom_path;
		backup_path += ".bak";
		std::error_code backup_error;
		if (!std::filesystem::exists(backup_path, backup_error))
		{
			std::filesystem::copy_file(
				saved_rom_path,
				backup_path,
				std::filesystem::copy_options::none,
				backup_error
			);
		}
		if (backup_error)
		{
			rom.m_buffer = original_rom_buffer;
			m_bonus_stage_status = "Could not create ROM backup: " +
				backup_error.message();
			return;
		}

		rom.SaveROM();

		std::ifstream saved_rom_file(saved_rom_path, std::ios::binary);
		if (!saved_rom_file)
		{
			m_bonus_stage_status =
				"Import changed the in-memory ROM, but the saved ROM could not be reopened: " +
				PathToUtf8(saved_rom_path);
			return;
		}

		const std::vector<Uint8> disk_buffer{
			std::istreambuf_iterator<char>(saved_rom_file),
			std::istreambuf_iterator<char>{}
		};
		if (disk_buffer != rom.m_buffer)
		{
			m_bonus_stage_status =
				"Import changed the in-memory ROM, but disk verification failed. "
				"The file may be read-only or another process may have replaced it: " +
				PathToUtf8(saved_rom_path);
			return;
		}

		if (!rom.LoadROMFromPath(saved_rom_path))
		{
			m_bonus_stage_status =
				"The ROM was written and verified, but SpinTool could not reload it: " +
				PathToUtf8(saved_rom_path);
			return;
		}

		LoadBonusStageImages();

		std::error_code absolute_path_error;
		const std::filesystem::path absolute_saved_path =
			std::filesystem::absolute(saved_rom_path, absolute_path_error);
		const std::filesystem::path& displayed_path = absolute_path_error
			? saved_rom_path
			: absolute_saved_path;

		m_bonus_stage_status = import_result.message ; /*+
			". ROM saved, reloaded and verified from disk: " +
			PathToUtf8(displayed_path);*/
	}


	void EditorSpriteNavigator::LoadTailsPlaneImages()
	{
		m_result_display_mode = ResultDisplayMode::TAILS_PLANE_FRAMES;
		m_tails_plane_images.clear();
		m_tails_plane_warnings.clear();
		m_tails_plane_status.clear();
		m_selected_tails_image = -1;

		const rom::TailsPlaneDecodeResult decode_result =
			rom::TailsPlaneDecoder::Decode(m_owning_ui.GetROM());
		m_tails_plane_warnings = decode_result.warnings;
		if (!decode_result.error.empty())
		{
			m_tails_plane_status = "Error: " + decode_result.error;
			return;
		}

		const Uint8 selected_scene_mask = static_cast<Uint8>(
			1U << static_cast<unsigned int>(m_selected_tails_scene)
		);
		for (const rom::TailsPlaneFrame& decoded_frame : decode_result.frames)
		{
			if (!decoded_frame.sprite ||
				(decoded_frame.scene_mask & selected_scene_mask) == 0U)
			{
				continue;
			}
			std::shared_ptr<const rom::Sprite> frame_sprite = decoded_frame.sprite;
			TailsPlaneFramePreview preview;
			preview.name = decoded_frame.name;
			preview.usage = decoded_frame.usage;
			preview.frame_id = decoded_frame.frame_id;
			preview.scene_mask = decoded_frame.scene_mask;
			preview.texture = std::make_shared<UISpriteTexture>(frame_sprite);
			m_tails_plane_images.emplace_back(std::move(preview));
		}

		const auto& palettes = m_owning_ui.GetPalettes();
		if (!palettes.empty())
		{
			// Keep the palette currently selected by the user. Loading Tails-plane
			// frames must not silently switch the editor to palette 50.
			m_chosen_palette = std::clamp(
				m_chosen_palette,
				0,
				static_cast<int>(palettes.size()) - 1
			);
		}

		if (m_tails_plane_images.empty())
		{
			m_tails_plane_status = "No complete Tails plane frame could be decoded.";
		}
	}

	void EditorSpriteNavigator::ImportTailsPlaneImage(
		const std::filesystem::path& path,
		const std::size_t image_index
	)
	{
		m_result_display_mode = ResultDisplayMode::TAILS_PLANE_FRAMES;
		if (image_index >= m_tails_plane_images.size())
		{
			m_tails_plane_status = "The selected Tails plane frame no longer exists.";
			return;
		}

		const std::string path_utf8 = PathToUtf8(path);
		SDLSurfaceHandle loaded_image{ IMG_Load(path_utf8.c_str()) };
		if (!loaded_image)
		{
			m_tails_plane_status = "Could not load PNG: " + path_utf8;
			return;
		}

		const auto& palettes = m_owning_ui.GetPalettes();
		if (palettes.empty())
		{
			m_tails_plane_status = "No palette is loaded.";
			return;
		}
		m_chosen_palette = std::clamp(
			m_chosen_palette,
			0,
			static_cast<int>(palettes.size()) - 1
		);
		const TailsPlaneFramePreview target = m_tails_plane_images[image_index];
		if (!target.texture || !target.texture->sprite)
		{
			m_tails_plane_status = "The selected Tails plane frame has no sprite data.";
			return;
		}
		int preferred_width = 0;
		int preferred_height = 0;
		const std::vector<Uint8> preferred_indices = RenderSpriteIndexedPixels(
			*target.texture->sprite,
			*palettes[static_cast<std::size_t>(m_chosen_palette)],
			preferred_width,
			preferred_height
		);
		const std::vector<Uint8> indexed_pixels = ConvertSurfaceToIndexed(
			loaded_image.get(),
			*palettes[static_cast<std::size_t>(m_chosen_palette)],
			preferred_indices,
			preferred_width,
			preferred_height
		);
		if (indexed_pixels.empty())
		{
			m_tails_plane_status = "The PNG could not be converted to indexed pixels.";
			return;
		}

		rom::SpinballROM& working_rom = m_owning_ui.GetROM();
		const std::filesystem::path saved_rom_path = working_rom.m_filepath;
		if (saved_rom_path.empty())
		{
			m_tails_plane_status = "The working ROM has no file path and cannot be saved.";
			return;
		}
		const std::vector<Uint8> original_rom_buffer = working_rom.m_buffer;
		const std::filesystem::path reference_rom_path =
			m_owning_ui.GetReferenceROMPath();
		if (reference_rom_path.empty())
		{
			m_tails_plane_status =
				"No reference ROM is associated with the current working ROM.";
			return;
		}

		rom::SpinballROM reference_rom;
		if (!reference_rom.LoadROMFromPath(reference_rom_path))
		{
			m_tails_plane_status = "Could not load reference ROM: " +
				PathToUtf8(reference_rom_path);
			return;
		}

		const rom::TailsPlaneImportResult import_result =
			rom::TailsPlaneDecoder::ImportIndexedImage(
				working_rom,
				reference_rom,
				target.frame_id,
				indexed_pixels,
				loaded_image->w,
				loaded_image->h
			);
		if (!import_result.success)
		{
			m_tails_plane_status = "Import failed: " + import_result.message;
			return;
		}
		if (!import_result.changed)
		{
			m_tails_plane_status = import_result.message;
			return;
		}

		std::filesystem::path backup_path = saved_rom_path;
		backup_path += ".bak";
		std::error_code backup_error;
		if (!std::filesystem::exists(backup_path, backup_error))
		{
			std::filesystem::copy_file(
				saved_rom_path,
				backup_path,
				std::filesystem::copy_options::none,
				backup_error
			);
		}
		if (backup_error)
		{
			working_rom.m_buffer = original_rom_buffer;
			m_tails_plane_status = "Could not create ROM backup: " +
				backup_error.message();
			return;
		}

		working_rom.SaveROM();
		std::ifstream saved_rom_file(saved_rom_path, std::ios::binary);
		if (!saved_rom_file)
		{
			m_tails_plane_status =
				"Import changed the in-memory ROM, but the saved ROM could not be reopened: " +
				PathToUtf8(saved_rom_path);
			return;
		}
		const std::vector<Uint8> disk_buffer{
			std::istreambuf_iterator<char>(saved_rom_file),
			std::istreambuf_iterator<char>{}
		};
		if (disk_buffer != working_rom.m_buffer)
		{
			m_tails_plane_status =
				"Import changed the in-memory ROM, but disk verification failed: " +
				PathToUtf8(saved_rom_path);
			return;
		}
		if (!working_rom.LoadROMFromPath(saved_rom_path))
		{
			m_tails_plane_status =
				"The ROM was written and verified, but SpinTool could not reload it: " +
				PathToUtf8(saved_rom_path);
			return;
		}

		LoadTailsPlaneImages();
		m_tails_plane_status = import_result.message;
	}

	void EditorSpriteNavigator::ExportTailsPlaneImage(const std::size_t image_index)
	{
		// Exporting a valid Tails frame is intentionally silent.  Clear any
		// previous status first, while still reporting errors below.
		m_tails_plane_status.clear();

		if (image_index >= m_tails_plane_images.size())
		{
			m_tails_plane_status = "The selected Tails plane frame no longer exists.";
			return;
		}
		const TailsPlaneFramePreview& image = m_tails_plane_images[image_index];
		if (!image.texture || !image.texture->sprite)
		{
			m_tails_plane_status = "The selected Tails plane frame has no image.";
			return;
		}
		const auto& palettes = m_owning_ui.GetPalettes();
		if (palettes.empty())
		{
			m_tails_plane_status = "No palette is loaded.";
			return;
		}
		m_chosen_palette = std::clamp(
			m_chosen_palette,
			0,
			static_cast<int>(palettes.size()) - 1
		);

		const char* scene_slug = "ending";
		switch (m_selected_tails_scene)
		{
		case 0: scene_slug = "intro"; break;
		case 1: scene_slug = "after_start"; break;
		case 2: scene_slug = "before_demo"; break;
		default: break;
		}
		char filename[160]{};
		snprintf(
			filename,
			sizeof(filename),
			"tails_plane_%s_frame_%02zu_id_%02zu.png",
			scene_slug,
			image_index,
			image.frame_id
		);
		std::filesystem::path export_path =
			m_owning_ui.GetSpriteExportPath().append(filename);
		std::error_code directory_error;
		std::filesystem::create_directories(export_path.parent_path(), directory_error);
		if (directory_error)
		{
			m_tails_plane_status = "Could not create export directory: " +
				directory_error.message();
			return;
		}

		SDLPaletteHandle palette = Renderer::CreateSDLPalette(
			*palettes[static_cast<std::size_t>(m_chosen_palette)]
		);
		const BoundingBox bounds = image.texture->sprite->GetBoundingBox();
		SDLSurfaceHandle output_surface{ SDL_CreateSurface(
			bounds.Width(),
			bounds.Height(),
			SDL_PIXELFORMAT_INDEX8
		) };
		if (!output_surface)
		{
			m_tails_plane_status = "Could not create the PNG export surface.";
			return;
		}
		SDL_SetSurfacePalette(output_surface.get(), palette.get());
		SDL_SetSurfaceColorKey(output_surface.get(), true, 0);
		image.texture->sprite->RenderToSurface(output_surface.get());
		const std::string export_path_utf8 = PathToUtf8(export_path);
		if (!IMG_SavePNG(output_surface.get(), export_path_utf8.c_str()))
		{
			m_tails_plane_status = "Could not export PNG: " + export_path_utf8;
			return;
		}
	}



	void EditorSpriteNavigator::LoadTitleScreenImages()
	{
		m_result_display_mode = ResultDisplayMode::TITLE_SCREEN_FRAMES;
		m_title_screen_images.clear();
		m_title_screen_warnings.clear();
		m_title_screen_status.clear();
		m_title_screen_palette_set.reset();
		m_selected_title_image = -1;

		const rom::TitleScreenDecodeResult decode_result =
			rom::TitleScreenDecoder::Decode(m_owning_ui.GetROM());
		m_title_screen_warnings = decode_result.warnings;
		m_title_screen_palette_set = decode_result.palette_set;
		if (!decode_result.error.empty())
		{
			m_title_screen_status = "Error: " + decode_result.error;
			return;
		}

		for (const rom::TitleScreenFrame& decoded_frame : decode_result.frames)
		{
			if (!decoded_frame.sprite)
			{
				continue;
			}
			std::shared_ptr<const rom::Sprite> frame_sprite = decoded_frame.sprite;
			TitleScreenFramePreview preview;
			preview.category = decoded_frame.category;
			preview.name = decoded_frame.name;
			preview.usage = decoded_frame.usage;
			preview.frame_id = decoded_frame.frame_id;
			preview.palette_line_map = decoded_frame.palette_line_map;
			preview.texture = std::make_shared<UISpriteTexture>(frame_sprite);
			m_title_screen_images.emplace_back(std::move(preview));
		}

		if (!m_title_screen_palette_set)
		{
			m_title_screen_status = "The title-screen palette set could not be loaded.";
			m_title_screen_images.clear();
			return;
		}
		if (m_title_screen_images.empty())
		{
			m_title_screen_status = "No complete title-screen frame could be decoded.";
		}
	}

	void EditorSpriteNavigator::ImportTitleScreenImage(
		const std::filesystem::path& path,
		const std::size_t image_index
	)
	{
		m_result_display_mode = ResultDisplayMode::TITLE_SCREEN_FRAMES;
		if (image_index >= m_title_screen_images.size())
		{
			m_title_screen_status = "The selected title-screen frame no longer exists.";
			return;
		}

		const std::string path_utf8 = PathToUtf8(path);
		SDLSurfaceHandle loaded_image{ IMG_Load(path_utf8.c_str()) };
		if (!loaded_image)
		{
			m_title_screen_status = "Could not load PNG: " + path_utf8;
			return;
		}

		const TitleScreenFramePreview target = m_title_screen_images[image_index];
		if (!m_title_screen_palette_set)
		{
			m_title_screen_status = "The four title-screen palettes are not loaded.";
			return;
		}
		if (!target.texture || !target.texture->sprite)
		{
			m_title_screen_status = "The selected title-screen frame has no sprite data.";
			return;
		}
		int preferred_width = 0;
		int preferred_height = 0;
		const std::vector<Uint8> preferred_indices = RenderSpriteIndexedPixels(
			*target.texture->sprite,
			*m_title_screen_palette_set,
			preferred_width,
			preferred_height
		);
		const std::vector<Uint8> indexed_pixels = ConvertSurfaceToTitleIndexed(
			loaded_image.get(),
			*m_title_screen_palette_set,
			target.palette_line_map,
			preferred_indices,
			preferred_width,
			preferred_height
		);
		if (indexed_pixels.empty())
		{
			m_title_screen_status = "The PNG could not be converted to indexed pixels.";
			return;
		}

		rom::SpinballROM& working_rom = m_owning_ui.GetROM();
		const std::filesystem::path saved_rom_path = working_rom.m_filepath;
		if (saved_rom_path.empty())
		{
			m_title_screen_status = "The working ROM has no file path and cannot be saved.";
			return;
		}
		const std::vector<Uint8> original_rom_buffer = working_rom.m_buffer;
		const std::filesystem::path reference_rom_path =
			m_owning_ui.GetReferenceROMPath();
		if (reference_rom_path.empty())
		{
			m_title_screen_status =
				"No reference ROM is associated with the current working ROM.";
			return;
		}

		rom::SpinballROM reference_rom;
		if (!reference_rom.LoadROMFromPath(reference_rom_path))
		{
			m_title_screen_status = "Could not load reference ROM: " +
				PathToUtf8(reference_rom_path);
			return;
		}

		const rom::TitleScreenImportResult import_result =
			rom::TitleScreenDecoder::ImportIndexedImage(
				working_rom,
				reference_rom,
				target.frame_id,
				indexed_pixels,
				loaded_image->w,
				loaded_image->h
			);
		if (!import_result.success)
		{
			m_title_screen_status = "Import failed: " + import_result.message;
			return;
		}
		if (!import_result.changed)
		{
			m_title_screen_status = import_result.message;
			return;
		}

		std::filesystem::path backup_path = saved_rom_path;
		backup_path += ".bak";
		std::error_code backup_error;
		if (!std::filesystem::exists(backup_path, backup_error))
		{
			std::filesystem::copy_file(
				saved_rom_path,
				backup_path,
				std::filesystem::copy_options::none,
				backup_error
			);
		}
		if (backup_error)
		{
			working_rom.m_buffer = original_rom_buffer;
			m_title_screen_status = "Could not create ROM backup: " +
				backup_error.message();
			return;
		}

		working_rom.SaveROM();
		std::ifstream saved_rom_file(saved_rom_path, std::ios::binary);
		if (!saved_rom_file)
		{
			m_title_screen_status =
				"Import changed the in-memory ROM, but the saved ROM could not be reopened: " +
				PathToUtf8(saved_rom_path);
			return;
		}
		const std::vector<Uint8> disk_buffer{
			std::istreambuf_iterator<char>(saved_rom_file),
			std::istreambuf_iterator<char>{}
		};
		if (disk_buffer != working_rom.m_buffer)
		{
			m_title_screen_status =
				"Import changed the in-memory ROM, but disk verification failed: " +
				PathToUtf8(saved_rom_path);
			return;
		}
		if (!working_rom.LoadROMFromPath(saved_rom_path))
		{
			m_title_screen_status =
				"The ROM was written and verified, but SpinTool could not reload it: " +
				PathToUtf8(saved_rom_path);
			return;
		}

		LoadTitleScreenImages();
		m_title_screen_status = import_result.message;
	}

	void EditorSpriteNavigator::ExportTitleScreenImage(const std::size_t image_index)
	{
		m_title_screen_status.clear();
		if (image_index >= m_title_screen_images.size())
		{
			m_title_screen_status = "The selected title-screen frame no longer exists.";
			return;
		}
		const TitleScreenFramePreview& image = m_title_screen_images[image_index];
		if (!image.texture || !image.texture->sprite)
		{
			m_title_screen_status = "The selected title-screen frame has no image.";
			return;
		}
		if (!m_title_screen_palette_set)
		{
			m_title_screen_status = "The four title-screen palettes are not loaded.";
			return;
		}

		char filename[160]{};
		snprintf(
			filename,
			sizeof(filename),
			"title_screen_%s_frame_%02zu_id_%03zu.png",
			TitleCategorySlug(image.category),
			image_index,
			image.frame_id
		);
		std::filesystem::path export_path =
			m_owning_ui.GetSpriteExportPath().append(filename);
		std::error_code directory_error;
		std::filesystem::create_directories(export_path.parent_path(), directory_error);
		if (directory_error)
		{
			m_title_screen_status = "Could not create export directory: " +
				directory_error.message();
			return;
		}

		SDLPaletteHandle palette =
			Renderer::CreateSDLPaletteForSet(*m_title_screen_palette_set);
		if (!palette)
		{
			m_title_screen_status = "Could not create the combined title-screen palette.";
			return;
		}
		const BoundingBox bounds = image.texture->sprite->GetBoundingBox();
		SDLSurfaceHandle output_surface{ SDL_CreateSurface(
			bounds.Width(),
			bounds.Height(),
			SDL_PIXELFORMAT_INDEX8
		) };
		if (!output_surface)
		{
			m_title_screen_status = "Could not create the PNG export surface.";
			return;
		}
		SDL_SetSurfacePalette(output_surface.get(), palette.get());
		SDL_SetSurfaceColorKey(output_surface.get(), true, 0);
		image.texture->sprite->RenderToSurface(output_surface.get());
		const std::string export_path_utf8 = PathToUtf8(export_path);
		if (!IMG_SavePNG(output_surface.get(), export_path_utf8.c_str()))
		{
			m_title_screen_status = "Could not export PNG: " + export_path_utf8;
		}
	}

	void EditorSpriteNavigator::ImportMainSpriteImage(
		const std::filesystem::path& path,
		Uint32 sprite_rom_offset
	)
	{
		m_result_display_mode = ResultDisplayMode::MAIN_SPRITES;
		m_main_sprite_status.clear();

		rom::SpinballROM& rom = m_owning_ui.GetROM();
		std::shared_ptr<const rom::Sprite> target_sprite =
			rom::Sprite::LoadFromROM(rom, sprite_rom_offset);
		if (!target_sprite)
		{
			m_main_sprite_status = "The selected Main Sprite is no longer valid at offset 0x";
			char offset_text[16]{};
			snprintf(offset_text, sizeof(offset_text), "%06X", static_cast<unsigned int>(sprite_rom_offset));
			m_main_sprite_status += offset_text;
			return;
		}

		const std::string path_utf8 = PathToUtf8(path);
		SDLSurfaceHandle loaded_image{ IMG_Load(path_utf8.c_str()) };
		if (!loaded_image)
		{
			m_main_sprite_status = "Could not load PNG: " + path_utf8;
			return;
		}

		const auto& palettes = m_owning_ui.GetPalettes();
		if (palettes.empty())
		{
			m_main_sprite_status = "No palette is loaded.";
			return;
		}
		m_chosen_palette = std::clamp(
			m_chosen_palette,
			0,
			static_cast<int>(palettes.size()) - 1
		);
		int preferred_width = 0;
		int preferred_height = 0;
		const std::vector<Uint8> preferred_indices = RenderSpriteIndexedPixels(
			*target_sprite,
			*palettes[static_cast<std::size_t>(m_chosen_palette)],
			preferred_width,
			preferred_height
		);
		const std::vector<Uint8> indexed_pixels = ConvertSurfaceToIndexed(
			loaded_image.get(),
			*palettes[static_cast<std::size_t>(m_chosen_palette)],
			preferred_indices,
			preferred_width,
			preferred_height
		);
		if (indexed_pixels.empty())
		{
			m_main_sprite_status = "The PNG could not be converted to indexed pixels.";
			return;
		}

		const BoundingBox bounds = target_sprite->GetBoundingBox();
		if (bounds.Width() <= 0 || bounds.Height() <= 0)
		{
			m_main_sprite_status = "The selected Main Sprite has invalid dimensions.";
			return;
		}

		const std::size_t header_size = 4U +
			target_sprite->sprite_tiles.size() * 6U;
		std::size_t pixel_data_size = 0;
		for (const auto& sprite_tile : target_sprite->sprite_tiles)
		{
			if (!sprite_tile || sprite_tile->x_size == 0 || sprite_tile->y_size == 0)
			{
				m_main_sprite_status = "The selected Main Sprite contains an invalid piece.";
				return;
			}
			pixel_data_size +=
				(static_cast<std::size_t>(sprite_tile->x_size) *
				 static_cast<std::size_t>(sprite_tile->y_size)) / 2U;
		}

		const std::size_t write_begin =
			static_cast<std::size_t>(sprite_rom_offset) + header_size;
		if (
			write_begin > rom.m_buffer.size() ||
			pixel_data_size > rom.m_buffer.size() - write_begin
		)
		{
			m_main_sprite_status = "The Main Sprite pixel data extends outside the ROM.";
			return;
		}

		if (!rom.m_filepath.empty())
		{
			std::filesystem::path backup_path = rom.m_filepath;
			backup_path += ".bak";
			std::error_code backup_error;
			if (!std::filesystem::exists(backup_path, backup_error))
			{
				std::filesystem::copy_file(
					rom.m_filepath,
					backup_path,
					std::filesystem::copy_options::none,
					backup_error
				);
			}
			if (backup_error)
			{
				m_main_sprite_status = "Could not create ROM backup: " +
					backup_error.message();
				return;
			}
		}

		std::size_t write_cursor = write_begin;
		for (const auto& sprite_tile : target_sprite->sprite_tiles)
		{
			const int piece_width = static_cast<int>(sprite_tile->x_size);
			const int piece_height = static_cast<int>(sprite_tile->y_size);
			const int piece_x = static_cast<int>(sprite_tile->x_offset) - bounds.min.x;
			const int piece_y = static_cast<int>(sprite_tile->y_offset) - bounds.min.y;

			auto read_palette_index = [&](int unflipped_x, int unflipped_y) -> Uint8
			{
				const int displayed_local_x = sprite_tile->blit_settings.flip_horizontal
					? piece_width - 1 - unflipped_x
					: unflipped_x;
				const int displayed_local_y = sprite_tile->blit_settings.flip_vertical
					? piece_height - 1 - unflipped_y
					: unflipped_y;
				const int source_x = piece_x + displayed_local_x;
				const int source_y = piece_y + displayed_local_y;
				if (
					source_x < 0 || source_y < 0 ||
					source_x >= loaded_image->w || source_y >= loaded_image->h
				)
				{
					return 0;
				}
				const std::size_t source_index =
					static_cast<std::size_t>(source_y) *
					static_cast<std::size_t>(loaded_image->w) +
					static_cast<std::size_t>(source_x);
				return source_index < indexed_pixels.size()
					? static_cast<Uint8>(indexed_pixels[source_index] & 0x0FU)
					: 0;
			};

			for (int y = 0; y < piece_height; ++y)
			{
				for (int x = 0; x < piece_width; x += 2)
				{
					const Uint8 left = read_palette_index(x, y);
					const Uint8 right = read_palette_index(x + 1, y);
					rom.m_buffer[write_cursor++] = static_cast<Uint8>(
						(left << 4U) | right
					);
				}
			}
		}

		UpdateMegaDriveChecksum(rom);

		const std::filesystem::path saved_rom_path = rom.m_filepath;
		if (saved_rom_path.empty())
		{
			m_main_sprite_status =
				"Import succeeded in memory, but the ROM has no file path and could not be saved.";
			return;
		}

		rom.SaveROM();

		std::ifstream saved_rom_file(saved_rom_path, std::ios::binary);
		if (!saved_rom_file)
		{
			m_main_sprite_status =
				"Import changed the in-memory ROM, but the saved ROM could not be reopened: " +
				PathToUtf8(saved_rom_path);
			return;
		}
		const std::vector<Uint8> disk_buffer{
			std::istreambuf_iterator<char>(saved_rom_file),
			std::istreambuf_iterator<char>{}
		};
		if (disk_buffer != rom.m_buffer)
		{
			m_main_sprite_status =
				"Import changed the in-memory ROM, but disk verification failed: " +
				PathToUtf8(saved_rom_path);
			return;
		}

		if (!rom.LoadROMFromPath(saved_rom_path))
		{
			m_main_sprite_status =
				"The ROM was written and verified, but SpinTool could not reload it: " +
				PathToUtf8(saved_rom_path);
			return;
		}

		std::shared_ptr<const rom::Sprite> refreshed_sprite =
			rom::Sprite::LoadFromROM(rom, sprite_rom_offset);
		if (!refreshed_sprite)
		{
			m_main_sprite_status =
				"The ROM was saved, but the modified sprite could not be decoded again.";
			return;
		}

		{
			std::lock_guard<std::recursive_mutex> render_lock(
				m_owning_ui.m_render_to_texture_mutex
			);
			for (std::shared_ptr<UISpriteTexture>& texture : m_sprites_found)
			{
				if (
					texture && texture->sprite &&
					texture->sprite->rom_data.rom_offset == sprite_rom_offset
				)
				{
					texture = std::make_shared<UISpriteTexture>(refreshed_sprite);
					texture->texture = texture->RenderTextureForPalette(
						*palettes[static_cast<std::size_t>(m_chosen_palette)]
					);
					break;
				}
			}
		}

		m_selected_sprite_rom_offset = sprite_rom_offset;
		std::error_code absolute_path_error;
		const std::filesystem::path absolute_saved_path =
			std::filesystem::absolute(saved_rom_path, absolute_path_error);
		const std::filesystem::path& displayed_path = absolute_path_error
			? saved_rom_path
			: absolute_saved_path;

		std::ostringstream status;
		status << "Main Sprite imported at 0x"
			<< std::hex << std::uppercase << sprite_rom_offset
			<< std::dec; /*<< ". ROM saved, reloaded and verified: "
			<< PathToUtf8(displayed_path);*/
		if (loaded_image->w != bounds.Width() || loaded_image->h != bounds.Height())
		{
			status << ". PNG size was " << loaded_image->w << "x" << loaded_image->h
				<< "; sprite area is " << bounds.Width() << "x" << bounds.Height()
				<< " (extra pixels ignored, missing pixels made transparent)";
		}
		m_main_sprite_status = status.str();
	}


	void EditorSpriteNavigator::InvalidatePaletteDependentTextures()
	{
		for (std::shared_ptr<UISpriteTexture>& texture : m_sprites_found)
		{
			if (texture)
			{
				texture->texture.reset();
			}
		}

		for (BonusStageImagePreview& image : m_bonus_stage_images)
		{
			if (image.texture)
			{
				image.texture->texture.reset();
			}
		}

		for (TailsPlaneFramePreview& image : m_tails_plane_images)
		{
			if (image.texture)
			{
				image.texture->texture.reset();
			}
		}

		for (TitleScreenFramePreview& image : m_title_screen_images)
		{
			if (image.texture)
			{
				image.texture->texture.reset();
			}
		}
		if (!m_title_screen_images.empty())
		{
			const rom::TitleScreenDecodeResult refreshed_title =
				rom::TitleScreenDecoder::Decode(m_owning_ui.GetROM());
			if (refreshed_title.palette_set)
			{
				m_title_screen_palette_set = refreshed_title.palette_set;
			}
		}

		m_random_texture.reset();
		m_attempt_render_of_arbitrary_data = true;
	}

	void EditorSpriteNavigator::Update()
	{
		if (m_visible == false)
		{
			return;
		}

		if (m_show_arbitrary_render && m_random_texture != nullptr)
		{
			ImGui::SetNextWindowSize(ImVec2{ -1, -1 }, ImGuiCond_Always);
			if (ImGui::Begin(
				"Arbitrary texture preview",
				&m_show_arbitrary_render,
				ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing
			))
			{
				ImGui::Image(
					(ImTextureID)m_random_texture.get(),
					{ m_random_texture->w * m_zoom, m_random_texture->h * m_zoom }
				);
			}
			ImGui::End();
		}

		if (ImGui::Begin("Sprite Discoverator", &m_visible))
		{
			static char path_buffer[4096];

			const size_t rom_size = m_owning_ui.GetROM().m_buffer.size();
			const Uint32 maximum_scan_offset = rom_size == 0
				? 0
				: static_cast<Uint32>(std::min<size_t>(
					rom_size - 1,
					static_cast<size_t>(std::numeric_limits<Uint32>::max())
				));

			if (m_scan_end_offset == 0)
			{
				m_scan_end_offset = std::min<Uint32>(0x03909D, maximum_scan_offset);
			}
			m_offset = std::min(m_offset, maximum_scan_offset);
			m_scan_start_offset = std::min(m_scan_start_offset, maximum_scan_offset);
			m_scan_end_offset = std::min(m_scan_end_offset, maximum_scan_offset);

			const Uint32 current_scan_start = m_scan_start_offset;
			const Uint32 current_scan_end = m_scan_end_offset;

			ImGui::SeparatorText("Global Settings");
			if (ImGui::RadioButton(
				"Render 8x8 VDP Tiles",
				m_render_arbitrary_with_palette == ArbitraryRenderMode::VDP_TILES
			))
			{
				m_render_arbitrary_with_palette = ArbitraryRenderMode::VDP_TILES;
				m_attempt_render_of_arbitrary_data = true;
			}
			ImGui::SameLine();
			if (ImGui::RadioButton(
				"Render 16-bit Colour",
				m_render_arbitrary_with_palette == ArbitraryRenderMode::VDP_COLOUR
			))
			{
				m_render_arbitrary_with_palette = ArbitraryRenderMode::VDP_COLOUR;
				m_attempt_render_of_arbitrary_data = true;
			}
			ImGui::SameLine();
			if (ImGui::RadioButton(
				"Render Palette Colour",
				m_render_arbitrary_with_palette == ArbitraryRenderMode::PALETTE
			))
			{
				m_render_arbitrary_with_palette = ArbitraryRenderMode::PALETTE;
				m_attempt_render_of_arbitrary_data = true;
			}
			ImGui::SameLine();
			if (ImGui::Checkbox("Display arbitrary render preview", &m_show_arbitrary_render))
			{
				m_attempt_render_of_arbitrary_data = true;
				m_random_texture.reset();
			}

			if (m_render_arbitrary_with_palette == ArbitraryRenderMode::VDP_TILES)
			{
				ImGui::SetNextItemWidth(128);
				m_attempt_render_of_arbitrary_data |= ImGui::InputInt(
					"Width",
					&m_arbitrary_num_tiles_width,
					1,
					10,
					ImGuiInputTextFlags_EnterReturnsTrue
				);
				ImGui::SameLine();
				ImGui::SetNextItemWidth(128);
				m_attempt_render_of_arbitrary_data |= ImGui::InputInt(
					"Height",
					&m_arbitrary_num_tiles_height,
					1,
					10,
					ImGuiInputTextFlags_EnterReturnsTrue
				);
			}
			else
			{
				ImGui::SetNextItemWidth(128);
				m_attempt_render_of_arbitrary_data |= ImGui::InputInt(
					"Width",
					&m_arbitrary_texture_width,
					1,
					10,
					ImGuiInputTextFlags_EnterReturnsTrue
				);
				ImGui::SameLine();
				ImGui::SetNextItemWidth(128);
				m_attempt_render_of_arbitrary_data |= ImGui::InputInt(
					"Height",
					&m_arbitrary_texture_height,
					1,
					10,
					ImGuiInputTextFlags_EnterReturnsTrue
				);
			}

			m_arbitrary_num_tiles_width = std::max(1, m_arbitrary_num_tiles_width);
			m_arbitrary_num_tiles_height = std::max(1, m_arbitrary_num_tiles_height);
			m_arbitrary_texture_width = std::max(1, m_arbitrary_texture_width);
			m_arbitrary_texture_height = std::max(1, m_arbitrary_texture_height);

			if (m_attempt_render_of_arbitrary_data && m_show_arbitrary_render)
			{
				switch (m_render_arbitrary_with_palette)
				{
					case ArbitraryRenderMode::PALETTE:
						m_random_texture = Renderer::RenderArbitaryOffsetToTexture(
							m_owning_ui.GetROM(),
							m_offset,
							{ m_arbitrary_texture_width, m_arbitrary_texture_height },
							*m_owning_ui.GetPalettes().at(m_chosen_palette)
						);
						break;
					case ArbitraryRenderMode::VDP_COLOUR:
						m_random_texture = Renderer::RenderArbitaryOffsetToTexture(
							m_owning_ui.GetROM(),
							m_offset,
							{ m_arbitrary_texture_width, m_arbitrary_texture_height }
						);
						break;
					case ArbitraryRenderMode::VDP_TILES:
						m_random_texture = Renderer::RenderArbitaryOffsetToTilesetTexture(
							m_owning_ui.GetROM(),
							m_offset,
							{ m_arbitrary_num_tiles_width, m_arbitrary_num_tiles_height }
						);
						break;
				}
				m_attempt_render_of_arbitrary_data = false;
			}

			ImGui::SeparatorText("Bonus Object Frames");
			static const char* bonus_stage_names[] =
			{
				"ROBO SMILE",
				"CLUCKER'S DEFENSE",
				"TRAPPED ALIVE",
				"THE MARCH",
			};

			ImGui::SetNextItemWidth(220.0f);
			ImGui::Combo(
				"Stage##bonus_stage",
				&m_selected_bonus_stage,
				bonus_stage_names,
				4
			);
			ImGui::SameLine();
			if (ImGui::Button("Load Bonus Images"))
			{
				LoadBonusStageImages();
			}
			ImGui::SameLine();
			if (ImGui::Button("Clear Bonus Images"))
			{
				m_result_display_mode = ResultDisplayMode::BONUS_OBJECT_FRAMES;
				m_bonus_stage_images.clear();
				m_bonus_stage_warnings.clear();
				m_bonus_stage_status.clear();
				m_selected_bonus_image = -1;
			}
			if (!m_bonus_stage_status.empty())
			{
				ImGui::TextWrapped("%s", m_bonus_stage_status.c_str());
			}


			ImGui::SeparatorText("Tails Plane / Cinematic Frames");
			static const char* tails_scene_names[] =
			{
				"Betore title screen",
				"Before last level",
				"Before gameplay demo",
				"Ending cutscene",
			};
			ImGui::SetNextItemWidth(220.0f);
			ImGui::Combo(
				"Scene##tails_plane_scene",
				&m_selected_tails_scene,
				tails_scene_names,
				4
			);
			ImGui::SameLine();
			if (ImGui::Button("Load TPC Images"))
			{
				LoadTailsPlaneImages();
			}
			ImGui::SameLine();
			if (ImGui::Button("Clear TPC Images"))
			{
				m_result_display_mode = ResultDisplayMode::TAILS_PLANE_FRAMES;
				m_tails_plane_images.clear();
				m_tails_plane_warnings.clear();
				m_tails_plane_status.clear();
				m_selected_tails_image = -1;
			}
			if (!m_tails_plane_status.empty())
			{
				ImGui::TextWrapped("%s", m_tails_plane_status.c_str());
			}
			if (!m_tails_plane_warnings.empty() && ImGui::TreeNode("Decoder notes"))
			{
				for (const std::string& warning : m_tails_plane_warnings)
				{
					ImGui::BulletText("%s", warning.c_str());
				}
				ImGui::TreePop();
			}


			ImGui::SeparatorText("Title Screen Frames");
			static const char* title_category_names[] =
			{
				"Sonic",
				"Bumper / Ring",
				"Logo Sonic",
				"Logo The Hedgehog",
				"Logo Spinball",
			};
			ImGui::SetNextItemWidth(220.0f);
			if (ImGui::Combo(
				"Category##title_screen_category",
				&m_selected_title_category,
				title_category_names,
				5
			))
			{
				m_result_display_mode = ResultDisplayMode::TITLE_SCREEN_FRAMES;
				m_selected_title_image = -1;
			}
			ImGui::SameLine();
			if (ImGui::Button("Load Title Images"))
			{
				LoadTitleScreenImages();
			}
			ImGui::SameLine();
			if (ImGui::Button("Clear Title Images"))
			{
				m_result_display_mode = ResultDisplayMode::TITLE_SCREEN_FRAMES;
				m_title_screen_images.clear();
				m_title_screen_warnings.clear();
				m_title_screen_status.clear();
				m_title_screen_palette_set.reset();
				m_selected_title_image = -1;
			}
			if (!m_title_screen_status.empty())
			{
				ImGui::TextWrapped("%s", m_title_screen_status.c_str());
			}
			if (!m_title_screen_warnings.empty() && ImGui::TreeNode("Title decoder notes"))
			{
				for (const std::string& warning : m_title_screen_warnings)
				{
					ImGui::BulletText("%s", warning.c_str());
				}
				ImGui::TreePop();
			}

			FileSelectorSettings bonus_import_settings;
			bonus_import_settings.object_typename = "Bonus Stage PNG";
			bonus_import_settings.target_directory = m_owning_ui.GetSpriteExportPath();
			bonus_import_settings.file_extension_filter = { ".png" };
			bonus_import_settings.tiled_previews = true;
			bonus_import_settings.num_columns = 4;
			bonus_import_settings.open_popup = std::exchange(
				m_open_bonus_import_popup,
				false
			);
			bonus_import_settings.close_popup = std::exchange(
				m_close_bonus_import_popup,
				false
			);
			const std::optional<std::filesystem::path> selected_bonus_png =
				DrawFileSelector(
					bonus_import_settings,
					m_owning_ui,
					std::nullopt
				);
			if (selected_bonus_png && m_bonus_import_target.has_value())
			{
				const std::size_t target_index = *m_bonus_import_target;
				m_bonus_import_target.reset();
				m_close_bonus_import_popup = true;
				ImportBonusStageImage(*selected_bonus_png, target_index);
			}


			FileSelectorSettings tails_import_settings;
			tails_import_settings.object_typename = "Tails Plane PNG";
			tails_import_settings.target_directory = m_owning_ui.GetSpriteExportPath();
			tails_import_settings.file_extension_filter = { ".png" };
			tails_import_settings.tiled_previews = true;
			tails_import_settings.num_columns = 4;
			tails_import_settings.open_popup = std::exchange(
				m_open_tails_import_popup,
				false
			);
			tails_import_settings.close_popup = std::exchange(
				m_close_tails_import_popup,
				false
			);
			const std::optional<std::filesystem::path> selected_tails_png =
				DrawFileSelector(
					tails_import_settings,
					m_owning_ui,
					std::nullopt
				);
			if (selected_tails_png && m_tails_import_target.has_value())
			{
				const std::size_t target_index = *m_tails_import_target;
				m_tails_import_target.reset();
				m_close_tails_import_popup = true;
				ImportTailsPlaneImage(*selected_tails_png, target_index);
			}


			FileSelectorSettings title_import_settings;
			title_import_settings.object_typename = "Title Screen PNG";
			title_import_settings.target_directory = m_owning_ui.GetSpriteExportPath();
			title_import_settings.file_extension_filter = { ".png" };
			title_import_settings.tiled_previews = true;
			title_import_settings.num_columns = 4;
			title_import_settings.open_popup = std::exchange(
				m_open_title_import_popup,
				false
			);
			title_import_settings.close_popup = std::exchange(
				m_close_title_import_popup,
				false
			);
			const std::optional<std::filesystem::path> selected_title_png =
				DrawFileSelector(
					title_import_settings,
					m_owning_ui,
					std::nullopt
				);
			if (selected_title_png && m_title_import_target.has_value())
			{
				const std::size_t target_index = *m_title_import_target;
				m_title_import_target.reset();
				m_close_title_import_popup = true;
				ImportTitleScreenImage(*selected_title_png, target_index);
			}

			FileSelectorSettings main_import_settings;
			main_import_settings.object_typename = "Main Sprite PNG";
			main_import_settings.target_directory = m_owning_ui.GetSpriteExportPath();
			main_import_settings.file_extension_filter = { ".png" };
			main_import_settings.tiled_previews = true;
			main_import_settings.num_columns = 4;
			main_import_settings.open_popup = std::exchange(
				m_open_main_import_popup,
				false
			);
			main_import_settings.close_popup = std::exchange(
				m_close_main_import_popup,
				false
			);
			const std::optional<std::filesystem::path> selected_main_png =
				DrawFileSelector(
					main_import_settings,
					m_owning_ui,
					std::nullopt
				);
			if (selected_main_png && m_main_import_target.has_value())
			{
				const Uint32 target_offset = *m_main_import_target;
				m_main_import_target.reset();
				m_close_main_import_popup = true;
				ImportMainSpriteImage(*selected_main_png, target_offset);
			}

			// Disabled in the redesigned Sprite Discoverator interface.
			// The implementation is intentionally preserved for possible future use.
#if 0

			if (ImGui::Button("Show Loaded Tilesets"))
			{
				for (const TilesetEntry& tileset_entry : m_owning_ui.GetTilesets())
				{
					const std::unique_ptr<rom::TileSet>& tileset = tileset_entry.tileset;
					ImGui::SeparatorText("Tileset");
					Uint32 offset = 0;
					while (true)
					{
						if (tileset->rom_data.rom_offset_end <= offset)
						{
							break;
						}
						std::shared_ptr<const rom::Sprite> new_sprite =
							tileset->CreateSpriteFromTile(offset);
						if (new_sprite == nullptr)
						{
							break;
						}
						offset = new_sprite->rom_data.rom_offset_end;
						m_sprites_found.emplace_back(
							std::make_shared<UISpriteTexture>(new_sprite)
						);
						m_sprites_found.back()->texture =
							m_sprites_found.back()->RenderTextureForPalette(
								*m_owning_ui.GetPalettes().at(m_chosen_palette)
							);
					}
				}
			}
#endif

			// Texture creation remains on the main/render thread.
			{
				std::vector<std::shared_ptr<UISpriteTexture>> ready_sprites;
				{
					std::lock_guard<std::mutex> pending_lock(m_pending_sprites_mutex);
					ready_sprites.swap(m_pending_sprites);
				}

				if (!ready_sprites.empty())
				{
					const auto& palettes = m_owning_ui.GetPalettes();
					if (!palettes.empty())
					{
						m_chosen_palette = std::clamp(
							m_chosen_palette,
							0,
							static_cast<int>(palettes.size()) - 1
						);
						const rom::Palette& palette =
							*palettes[static_cast<size_t>(m_chosen_palette)];
						for (auto& sprite : ready_sprites)
						{
							if (!sprite || !sprite->sprite)
							{
								continue;
							}

							if (
								sprite->sprite->rom_data.rom_offset < current_scan_start ||
								sprite->sprite->rom_data.rom_offset > current_scan_end ||
								sprite->sprite->rom_data.rom_offset_end > current_scan_end + 1
							)
							{
								continue;
							}

							sprite->texture = sprite->RenderTextureForPalette(palette);
							m_sprites_found.emplace_back(std::move(sprite));
						}
					}
				}
			}

			auto start_full_sprite_scan = [this](
				Uint32 requested_scan_start,
				Uint32 requested_scan_end
			)
			{
				const Uint32 scan_generation = ++m_scan_generation;
				m_find_all_running = true;
				m_find_all_progress = 0.0f;
				m_find_all_result_count = 0;

				m_sprites_found.clear();
				{
					std::lock_guard<std::mutex> pending_lock(m_pending_sprites_mutex);
					m_pending_sprites.clear();
				}
				m_selected_sprite_rom_offset = 0;

				std::thread([
					this,
					requested_scan_start,
					requested_scan_end,
					scan_generation
				]()
				{
					const size_t scan_rom_size = m_owning_ui.GetROM().m_buffer.size();
					if (scan_rom_size == 0)
					{
						if (m_scan_generation.load() == scan_generation)
						{
							m_find_all_progress = 1.0f;
							m_find_all_running = false;
						}
						return;
					}

					const Uint32 rom_last_offset =
						static_cast<Uint32>(scan_rom_size - 1);
					const Uint32 scan_start = std::min(
						requested_scan_start,
						rom_last_offset
					);
					const Uint32 scan_end = std::min(
						requested_scan_end,
						rom_last_offset
					);

					if (scan_start > scan_end)
					{
						if (m_scan_generation.load() == scan_generation)
						{
							m_find_all_progress = 1.0f;
							m_find_all_running = false;
						}
						return;
					}

					const Uint32 scan_length = scan_end - scan_start + 1U;
					Uint32 working_offset = scan_start;
					while (
						working_offset <= scan_end &&
						working_offset < scan_rom_size &&
						m_scan_generation.load() == scan_generation
					)
					{
						m_find_all_progress = static_cast<float>(
							working_offset - scan_start
						) / static_cast<float>(std::max<Uint32>(1U, scan_length));

						auto sprite = rom::Sprite::LoadFromROM(
							m_owning_ui.GetROM(),
							working_offset
						);

						if (sprite)
						{
							const Uint32 sprite_end = sprite->rom_data.rom_offset_end;
							if (
								sprite_end > working_offset &&
								sprite_end <= scan_rom_size &&
								sprite_end <= scan_end + 1 &&
								m_scan_generation.load() == scan_generation
							)
							{
								std::lock_guard<std::mutex> pending_lock(
									m_pending_sprites_mutex
								);
								if (m_scan_generation.load() == scan_generation)
								{
									m_pending_sprites.emplace_back(
										std::make_shared<UISpriteTexture>(sprite)
									);
									++m_find_all_result_count;
								}
							}
						}

						if (working_offset == std::numeric_limits<Uint32>::max())
						{
							break;
						}
						++working_offset;
					}

					if (m_scan_generation.load() == scan_generation)
					{
						m_find_all_progress = 1.0f;
						m_find_all_running = false;
					}
				}).detach();
			};

			ImGui::SeparatorText("Main Sprites");

			if (!m_main_sprite_status.empty())
			{
				ImGui::TextWrapped("%s", m_main_sprite_status.c_str());
			}

			ImGui::TextUnformatted("Start :");
			ImGui::SameLine(150.0f);
			ImGui::TextUnformatted("0x");
			ImGui::SameLine(0.0f, 2.0f);
			ImGui::SetNextItemWidth(160.0f);
			if (ImGui::InputScalar(
				"##m_scan_start_edit",
				ImGuiDataType_U32,
				&m_offset,
				nullptr,
				nullptr,
				"%06X",
				ImGuiInputTextFlags_CharsHexadecimal
			))
			{
				m_attempt_render_of_arbitrary_data = true;
			}

			ImGui::TextUnformatted("End :");
			ImGui::SameLine(150.0f);
			ImGui::TextUnformatted("0x");
			ImGui::SameLine(0.0f, 2.0f);
			ImGui::SetNextItemWidth(160.0f);
			ImGui::InputScalar(
				"##m_scan_end_offset",
				ImGuiDataType_U32,
				&m_scan_end_offset,
				nullptr,
				nullptr,
				"%06X",
				ImGuiInputTextFlags_CharsHexadecimal
			);

			if (ImGui::Button("Apply range and rescan"))
			{
				m_result_display_mode = ResultDisplayMode::MAIN_SPRITES;
				m_main_sprite_status.clear();
				m_main_import_target.reset();

				Uint32 requested_start = std::min(m_offset, maximum_scan_offset);
				Uint32 requested_end = std::min(m_scan_end_offset, maximum_scan_offset);
				if (requested_start > requested_end)
				{
					std::swap(requested_start, requested_end);
				}

				m_offset = requested_start;
				m_scan_start_offset = requested_start;
				m_scan_end_offset = requested_end;
				start_full_sprite_scan(
					m_scan_start_offset,
					m_scan_end_offset
				);
			}

			ImGui::SameLine();
			if (ImGui::Button("Clear Textures"))
			{
				m_result_display_mode = ResultDisplayMode::MAIN_SPRITES;
				m_main_sprite_status.clear();
				m_main_import_target.reset();
				++m_scan_generation;
				m_find_all_running = false;
				m_sprites_found.clear();
				{
					std::lock_guard<std::mutex> pending_lock(m_pending_sprites_mutex);
					m_pending_sprites.clear();
				}
				m_selected_sprite_rom_offset = 0;
			}
			
			if (m_find_all_running)
			{
				ImGui::ProgressBar(m_find_all_progress.load());
				ImGui::TextDisabled(
					"Scanning the selected ROM range; large ranges may take time."
				);
			}

			m_sprites_found.erase(
				std::remove_if(
					m_sprites_found.begin(),
					m_sprites_found.end(),
					[current_scan_start, current_scan_end](
						const std::shared_ptr<UISpriteTexture>& sprite
					)
					{
						return !sprite || !sprite->sprite ||
							sprite->sprite->rom_data.rom_offset < current_scan_start ||
							sprite->sprite->rom_data.rom_offset > current_scan_end ||
							sprite->sprite->rom_data.rom_offset_end > current_scan_end + 1;
					}
				),
				m_sprites_found.end()
			);


			ImGui::SeparatorText("Palette");

			if (m_result_display_mode == ResultDisplayMode::TITLE_SCREEN_FRAMES)
			{
				if (!m_title_screen_palette_set)
				{
					ImGui::TextDisabled("Load the title-screen frames to display their palettes.");
				}
				else
				{
					ImGui::TextUnformatted("Title-screen palette set (4 lines)");
					for (std::size_t line = 0U;
						line < m_title_screen_palette_set->palette_lines.size();
						++line)
					{
						const std::shared_ptr<rom::Palette>& palette =
							m_title_screen_palette_set->palette_lines[line];
						if (!palette) continue;
						ImGui::PushID(static_cast<int>(line));
						ImGui::Text(
							"Line %zu (ROM 0x%06X)",
							line,
							palette->offset
						);
						DrawPaletteSwatchPreview(*palette);
						ImGui::PopID();
					}
				}
			}
			else if (DrawPaletteSelectorWithPreview(m_chosen_palette, m_owning_ui))
			{
				for (std::shared_ptr<UISpriteTexture>& tex : m_sprites_found)
				{
					tex->texture.reset();
				}
				for (BonusStageImagePreview& image : m_bonus_stage_images)
				{
					if (image.texture) image.texture->texture.reset();
				}
				for (TailsPlaneFramePreview& image : m_tails_plane_images)
				{
					if (image.texture) image.texture->texture.reset();
				}
				m_attempt_render_of_arbitrary_data = true;
			}

			ImGui::SetNextItemWidth(260.0f);
			ImGui::SliderFloat("Zoom", &m_zoom, 1.0f, 8.0f, "%.1f");

			ImGui::SeparatorText("Results");

			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 2, 0 });
			ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2{ 2, 0 });

			const int cursor_start_x_pos = static_cast<int>(ImGui::GetCursorPosX());
			const int padding_x = static_cast<int>(ImGui::GetStyle().ItemSpacing.x);
			int current_width = cursor_start_x_pos;

			if (m_result_display_mode == ResultDisplayMode::BONUS_OBJECT_FRAMES)
			{
				ImGui::Text("Bonus Object Frames: %zu", m_bonus_stage_images.size());
				current_width = static_cast<int>(ImGui::GetCursorPosX());
				bool has_item_on_row = false;

				std::lock_guard<std::recursive_mutex> render_lock(
					m_owning_ui.m_render_to_texture_mutex
				);
				for (
					std::size_t image_index = 0;
					image_index < m_bonus_stage_images.size();
					++image_index
				)
				{
					BonusStageImagePreview& image = m_bonus_stage_images[image_index];
					std::shared_ptr<UISpriteTexture>& tex = image.texture;
					if (!tex || !tex->sprite)
					{
						continue;
					}

					if (tex->texture == nullptr)
					{
						tex->texture = tex->RenderTextureForPalette(
							*m_owning_ui.GetPalettes().at(m_chosen_palette)
						);
					}
					if (
						tex->dimensions.x <= 0 ||
						tex->dimensions.y <= 0 ||
						tex->texture == nullptr
					)
					{
						continue;
					}

					if (
						has_item_on_row &&
						ImGui::GetContentRegionAvail().x <
						current_width + padding_x + (tex->dimensions.x * m_zoom)
					)
					{
						ImGui::NewLine();
						ImGui::SameLine();
						ImGui::Dummy(ImVec2(0, 0));
						current_width = static_cast<int>(ImGui::GetCursorPosX());
						has_item_on_row = false;
					}
					if (has_item_on_row)
					{
						ImGui::SameLine();
					}

					ImGui::PushID(static_cast<int>(image_index));
					tex->DrawForImGui(m_zoom);
					current_width += static_cast<int>(
						(tex->dimensions.x * m_zoom) + ImGui::GetStyle().ItemSpacing.x
					);
					has_item_on_row = true;

					const bool hovered = ImGui::IsItemHovered();
					const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

					if (ImGui::BeginPopupContextItem(
						"bonus_stage_image_popup",
						ImGuiPopupFlags_MouseButtonRight
					))
					{
						if (ImGui::MenuItem("Import PNG into ROM"))
						{
							m_bonus_import_target = image_index;
							m_open_bonus_import_popup = true;
						}
						if (ImGui::MenuItem("Export image as PNG"))
						{
							snprintf(
								path_buffer,
								sizeof(path_buffer),
								"bonus_stage_%d_image_%03zu_mapping_%06X_base_%04X.png",
								m_selected_bonus_stage,
								image_index,
								static_cast<unsigned int>(image.mapping_offset),
								static_cast<unsigned int>(image.visual_tile_attributes)
							);
							std::filesystem::path export_path =
								m_owning_ui.GetSpriteExportPath().append(path_buffer);
							SDLPaletteHandle palette = Renderer::CreateSDLPalette(
								*m_owning_ui.GetPalettes().at(m_chosen_palette)
							);
							const BoundingBox bounds = tex->sprite->GetBoundingBox();
							SDLSurfaceHandle out_surface{ SDL_CreateSurface(
								bounds.Width(),
								bounds.Height(),
								SDL_PIXELFORMAT_INDEX8
							) };
							if (out_surface)
							{
								SDL_SetSurfacePalette(out_surface.get(), palette.get());
								SDL_SetSurfaceColorKey(out_surface.get(), true, 0);
								tex->sprite->RenderToSurface(out_surface.get());
								const std::string export_path_utf8 = PathToUtf8(export_path);
								IMG_SavePNG(out_surface.get(), export_path_utf8.c_str());
							}
						}
						ImGui::EndPopup();
					}

					if (m_selected_bonus_image == static_cast<int>(image_index))
					{
						ImGui::GetWindowDrawList()->AddRect(
							ImGui::GetItemRectMin(),
							ImGui::GetItemRectMax(),
							ImGui::GetColorU32(ImVec4{ 0, 192, 0, 255 }),
							1.0f,
							0,
							2.0f
						);
					}
					if (hovered)
					{
						ImGui::GetWindowDrawList()->AddRect(
							ImGui::GetItemRectMin(),
							ImGui::GetItemRectMax(),
							ImGui::GetColorU32(ImVec4{ 255, 255, 255, 255 }),
							1.0f,
							0,
							2.0f
						);
					}
					if (clicked)
					{
						m_selected_bonus_image = static_cast<int>(image_index);
						m_owning_ui.OpenSpriteViewer(tex->sprite);
					}
					ImGui::PopID();
				}
			}
			else if (m_result_display_mode == ResultDisplayMode::TAILS_PLANE_FRAMES)
			{
				// LoadTailsPlaneImages stores only the frames from the selected scene,
				// so frames belonging to the other three groups cannot leak into this list.
				ImGui::Text("Tails Plane / Cinematic Frames: %zu", m_tails_plane_images.size());
				current_width = static_cast<int>(ImGui::GetCursorPosX());
				bool has_item_on_row = false;

				std::lock_guard<std::recursive_mutex> render_lock(
					m_owning_ui.m_render_to_texture_mutex
				);
				for (std::size_t image_index = 0;
					image_index < m_tails_plane_images.size();
					++image_index)
				{
					TailsPlaneFramePreview& image = m_tails_plane_images[image_index];
					std::shared_ptr<UISpriteTexture>& tex = image.texture;
					if (!tex || !tex->sprite)
					{
						continue;
					}
					if (tex->texture == nullptr)
					{
						tex->texture = tex->RenderTextureForPalette(
							*m_owning_ui.GetPalettes().at(
								static_cast<std::size_t>(m_chosen_palette)
							)
						);
					}
					if (tex->dimensions.x <= 0 || tex->dimensions.y <= 0 || !tex->texture)
					{
						continue;
					}

					if (has_item_on_row &&
						ImGui::GetContentRegionAvail().x <
						current_width + padding_x + (tex->dimensions.x * m_zoom))
					{
						ImGui::NewLine();
						ImGui::SameLine();
						ImGui::Dummy(ImVec2(0, 0));
						current_width = static_cast<int>(ImGui::GetCursorPosX());
						has_item_on_row = false;
					}
					if (has_item_on_row)
					{
						ImGui::SameLine();
					}

					ImGui::PushID(static_cast<int>(image.frame_id));
					tex->DrawForImGui(m_zoom);
					current_width += static_cast<int>(
						(tex->dimensions.x * m_zoom) + ImGui::GetStyle().ItemSpacing.x
					);
					has_item_on_row = true;
					const bool hovered = ImGui::IsItemHovered();
					const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

					if (ImGui::BeginPopupContextItem(
						"tails_plane_frame_popup",
						ImGuiPopupFlags_MouseButtonRight
					))
					{
						if (ImGui::MenuItem("Import PNG into ROM"))
						{
							m_tails_import_target = image_index;
							m_open_tails_import_popup = true;
						}
						if (ImGui::MenuItem("Export image as PNG"))
						{
							ExportTailsPlaneImage(image_index);
						}
						ImGui::EndPopup();
					}

					if (m_selected_tails_image == static_cast<int>(image_index))
					{
						ImGui::GetWindowDrawList()->AddRect(
							ImGui::GetItemRectMin(),
							ImGui::GetItemRectMax(),
							ImGui::GetColorU32(ImVec4{ 0, 192, 0, 255 }),
							1.0f,
							0,
							2.0f
						);
					}
					if (hovered)
					{
						ImGui::GetWindowDrawList()->AddRect(
							ImGui::GetItemRectMin(),
							ImGui::GetItemRectMax(),
							ImGui::GetColorU32(ImVec4{ 255, 255, 255, 255 }),
							1.0f,
							0,
							2.0f
						);
					}
					if (clicked)
					{
						m_selected_tails_image = static_cast<int>(image_index);
						m_owning_ui.OpenSpriteViewer(tex->sprite);
					}
					ImGui::PopID();
				}
			}
			else if (m_result_display_mode == ResultDisplayMode::TITLE_SCREEN_FRAMES)
			{
				const rom::TitleScreenCategory selected_category =
					static_cast<rom::TitleScreenCategory>(m_selected_title_category);
				const std::size_t category_count = static_cast<std::size_t>(std::count_if(
					m_title_screen_images.begin(),
					m_title_screen_images.end(),
					[selected_category](const TitleScreenFramePreview& image)
					{
						return image.category == selected_category;
					}
				));
				ImGui::Text("%s: %zu", TitleCategoryName(selected_category), category_count);
				current_width = static_cast<int>(ImGui::GetCursorPosX());
				bool has_item_on_row = false;

				std::lock_guard<std::recursive_mutex> render_lock(
					m_owning_ui.m_render_to_texture_mutex
				);
				for (std::size_t image_index = 0;
					image_index < m_title_screen_images.size();
					++image_index)
				{
					TitleScreenFramePreview& image = m_title_screen_images[image_index];
					if (image.category != selected_category)
					{
						continue;
					}
					std::shared_ptr<UISpriteTexture>& tex = image.texture;
					if (!tex || !tex->sprite)
					{
						continue;
					}
					if (tex->texture == nullptr && m_title_screen_palette_set)
					{
						tex->texture = tex->RenderTextureForPaletteSet(
							*m_title_screen_palette_set
						);
					}
					if (tex->dimensions.x <= 0 || tex->dimensions.y <= 0 || !tex->texture)
					{
						continue;
					}

					if (has_item_on_row &&
						ImGui::GetContentRegionAvail().x <
						current_width + padding_x + (tex->dimensions.x * m_zoom))
					{
						ImGui::NewLine();
						ImGui::SameLine();
						ImGui::Dummy(ImVec2(0, 0));
						current_width = static_cast<int>(ImGui::GetCursorPosX());
						has_item_on_row = false;
					}
					if (has_item_on_row)
					{
						ImGui::SameLine();
					}

					ImGui::PushID(static_cast<int>(image.frame_id));
					tex->DrawForImGui(m_zoom);
					current_width += static_cast<int>(
						(tex->dimensions.x * m_zoom) + ImGui::GetStyle().ItemSpacing.x
					);
					has_item_on_row = true;
					const bool hovered = ImGui::IsItemHovered();
					const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

					if (ImGui::BeginPopupContextItem(
						"title_screen_frame_popup",
						ImGuiPopupFlags_MouseButtonRight
					))
					{
						if (ImGui::MenuItem("Import PNG into ROM"))
						{
							m_title_import_target = image_index;
							m_open_title_import_popup = true;
						}
						if (ImGui::MenuItem("Export image as PNG"))
						{
							ExportTitleScreenImage(image_index);
						}
						ImGui::EndPopup();
					}

					if (m_selected_title_image == static_cast<int>(image_index))
					{
						ImGui::GetWindowDrawList()->AddRect(
							ImGui::GetItemRectMin(),
							ImGui::GetItemRectMax(),
							ImGui::GetColorU32(ImVec4{ 0, 192, 0, 255 }),
							1.0f,
							0,
							2.0f
						);
					}
					if (hovered)
					{
						ImGui::GetWindowDrawList()->AddRect(
							ImGui::GetItemRectMin(),
							ImGui::GetItemRectMax(),
							ImGui::GetColorU32(ImVec4{ 255, 255, 255, 255 }),
							1.0f,
							0,
							2.0f
						);
					}
					if (clicked)
					{
						m_selected_title_image = static_cast<int>(image_index);
					}
					ImGui::PopID();
				}
			}
			else
			{
				ImGui::Text("Main Sprites: %zu", m_sprites_found.size());
				current_width = static_cast<int>(ImGui::GetCursorPosX());
				bool has_item_on_row = false;

				std::lock_guard<std::recursive_mutex> render_lock(
					m_owning_ui.m_render_to_texture_mutex
				);
				for (std::shared_ptr<UISpriteTexture>& tex : m_sprites_found)
				{
					if (!tex || !tex->sprite)
					{
						continue;
					}

					if (tex->texture == nullptr)
					{
						tex->texture = tex->RenderTextureForPalette(
							*m_owning_ui.GetPalettes().at(m_chosen_palette)
						);
					}
					if (tex->dimensions.x == 0 || tex->dimensions.y == 0)
					{
						continue;
					}

					if (
						has_item_on_row &&
						ImGui::GetContentRegionAvail().x <
						current_width + padding_x + (tex->dimensions.x * m_zoom)
					)
					{
						ImGui::NewLine();
						ImGui::SameLine();
						ImGui::Dummy(ImVec2(0, 0));
						current_width = static_cast<int>(ImGui::GetCursorPosX());
						has_item_on_row = false;
					}
					if (has_item_on_row)
					{
						ImGui::SameLine();
					}

					tex->DrawForImGui(m_zoom);
					current_width += static_cast<int>(
						(tex->dimensions.x * m_zoom) + ImGui::GetStyle().ItemSpacing.x
					);
					has_item_on_row = true;

					const bool hovered = ImGui::IsItemHovered();
					const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

					sprintf(
						path_buffer,
						"popup_%X02",
						static_cast<unsigned int>(tex->sprite->rom_data.rom_offset)
					);
					if (ImGui::BeginPopupContextItem(
						path_buffer,
						ImGuiPopupFlags_MouseButtonRight
					))
					{
						if (ImGui::MenuItem("Import PNG into ROM"))
						{
							m_main_import_target =
								tex->sprite->rom_data.rom_offset;
							m_open_main_import_popup = true;
						}

						sprintf(
							path_buffer,
							"Export image at 0x%X02",
							static_cast<unsigned int>(tex->sprite->rom_data.rom_offset)
						);
						if (ImGui::MenuItem(path_buffer))
						{
							sprintf(
								path_buffer,
								"spinball_image_%X02.png",
								static_cast<unsigned int>(tex->sprite->rom_data.rom_offset)
							);
							std::filesystem::path export_path =
								m_owning_ui.GetSpriteExportPath().append(path_buffer);
							SDLPaletteHandle palette = Renderer::CreateSDLPalette(
								*m_owning_ui.GetPalettes().at(static_cast<std::size_t>(m_chosen_palette))
							);
							SDLSurfaceHandle out_surface{ SDL_CreateSurface(
								tex->sprite->GetBoundingBox().Width(),
								tex->sprite->GetBoundingBox().Height(),
								SDL_PIXELFORMAT_INDEX8
							) };
							SDL_SetSurfacePalette(out_surface.get(), palette.get());
							SDL_SetSurfaceColorKey(out_surface.get(), true, 0);
							tex->sprite->RenderToSurface(out_surface.get());
							const std::string export_path_utf8 = PathToUtf8(export_path);
							assert(IMG_SavePNG(out_surface.get(), export_path_utf8.c_str()));
						}
						ImGui::EndPopup();
					}

					if (m_selected_sprite_rom_offset == tex->sprite->rom_data.rom_offset)
					{
						ImGui::GetWindowDrawList()->AddRect(
							ImGui::GetItemRectMin(),
							ImGui::GetItemRectMax(),
							ImGui::GetColorU32(ImVec4{ 0, 192, 0, 255 }),
							1.0f,
							0,
							2
						);
					}
					if (hovered)
					{
						ImGui::GetWindowDrawList()->AddRect(
							ImGui::GetItemRectMin(),
							ImGui::GetItemRectMax(),
							ImGui::GetColorU32(ImVec4{ 255, 255, 255, 255 }),
							1.0f,
							0,
							2
						);
					}
					if (clicked)
					{
						m_selected_sprite_rom_offset = tex->sprite->rom_data.rom_offset;
						m_owning_ui.OpenSpriteViewer(tex->sprite);
					}
				}
			}
			ImGui::PopStyleVar(2);
		}
		ImGui::End();
	}
}
