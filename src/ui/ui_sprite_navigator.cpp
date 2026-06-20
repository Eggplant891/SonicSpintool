#include "ui/ui_sprite_navigator.h"

#include "rom/spinball_rom.h"
#include "rom/bonus_stage_decoder.h"
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

	std::vector<Uint8> ConvertSurfaceToIndexed(
		SDL_Surface* source,
		const spintool::rom::Palette& palette
	)
	{
		std::vector<Uint8> output;
		if (!source || source->w <= 0 || source->h <= 0)
		{
			return output;
		}

		SDLSurfaceHandle converted{
			SDL_ConvertSurface(source, SDL_PIXELFORMAT_RGBA32)
		};
		if (!converted)
		{
			return output;
		}

		const SDL_PixelFormatDetails* format =
			SDL_GetPixelFormatDetails(converted->format);
		if (!format)
		{
			return output;
		}

		output.resize(
			static_cast<std::size_t>(converted->w) *
			static_cast<std::size_t>(converted->h),
			0
		);
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

				Uint8 best_index = 0;
				if (alpha >= 0x80U)
				{
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
						if (distance < best_distance)
						{
							best_distance = distance;
							best_index = palette_index;
						}
					}
				}
				output[
					static_cast<std::size_t>(y) * static_cast<std::size_t>(converted->w) +
					static_cast<std::size_t>(x)
				] = best_index;
			}
		}
		return output;
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
		const std::vector<Uint8> indexed_pixels = ConvertSurfaceToIndexed(
			loaded_image.get(),
			*palettes[static_cast<std::size_t>(m_chosen_palette)]
		);
		if (indexed_pixels.empty())
		{
			m_bonus_stage_status = "The PNG could not be converted to indexed pixels.";
			return;
		}

		rom::SpinballROM& rom = m_owning_ui.GetROM();
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
				m_bonus_stage_status = "Could not create ROM backup: " +
					backup_error.message();
				return;
			}
		}

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

		const BonusStageImagePreview target = m_bonus_stage_images[image_index];
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

		const std::filesystem::path saved_rom_path = rom.m_filepath;
		if (saved_rom_path.empty())
		{
			m_bonus_stage_status =
				"Import succeeded in memory, but the ROM has no file path and could not be saved.";
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
		const std::vector<Uint8> indexed_pixels = ConvertSurfaceToIndexed(
			loaded_image.get(),
			*palettes[static_cast<std::size_t>(m_chosen_palette)]
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
				"Render 126-bit Colour",
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
			if (ImGui::Button("Load Images"))
			{
				LoadBonusStageImages();
			}
			ImGui::SameLine();
			if (ImGui::Button("Clear Images"))
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


			ImGui::TextDisabled(
				"Applied scan range: 0x%06X - 0x%06X",
				static_cast<unsigned int>(m_scan_start_offset),
				static_cast<unsigned int>(m_scan_end_offset)
			);

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

			if (DrawPaletteSelectorWithPreview(m_chosen_palette, m_owning_ui))
			{
				for (std::shared_ptr<UISpriteTexture>& tex : m_sprites_found)
				{
					tex->texture.reset();
				}
				for (BonusStageImagePreview& image : m_bonus_stage_images)
				{
					if (image.texture)
					{
						image.texture->texture.reset();
					}
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
