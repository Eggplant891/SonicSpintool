#include "ui/ui_sprite_importer.h"

#include "rom/spinball_rom.h"
#include "ui/ui_editor.h"
#include "ui/ui_palette_viewer.h"
#include "ui/ui_file_selector.h"
#include "rom/sprite.h"

#include "SDL3/SDL_image.h"
#include "imgui.h"

#include <algorithm>
#include <iterator>
#include "rom/tileset.h"

namespace
{
	constexpr float img_scale = 2.0f;
}

namespace spintool
{
	void EditorImageImporter::InnerUpdate()
	{
		static char path_buffer[4096] = "";;
		bool update_preview = false;

		static FileSelectorSettings settings;
		settings.object_typename = "Image";
		settings.target_directory = m_owning_ui.GetSpriteExportPath();
		settings.file_extension_filter = { ".png", ".gif", ".bmp" };
		settings.tiled_previews = true;
		settings.num_columns = 4;

		std::optional<std::filesystem::path> selected_path = DrawFileSelector(settings, m_owning_ui, path_buffer);

		const bool path_changed = ImGui::InputText("Path", path_buffer, sizeof(path_buffer), ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::SameLine();
		settings.open_popup = ImGui::Button("Choose Image");
		settings.close_popup = false;

		if (m_available_palettes.empty())
		{
			return;
		}

		if(selected_path)
		{
			settings.close_popup = true;
			SDLSurfaceHandle loaded_surface{ IMG_Load(selected_path->generic_u8string().c_str()) };
			if (loaded_surface != nullptr)
			{
				m_loaded_path = path_buffer;
				m_imported_image = SDLSurfaceHandle{ SDL_ConvertSurface(loaded_surface.get(), SDL_PIXELFORMAT_RGBA32) };
				m_rendered_imported_image = Renderer::RenderToTexture(m_imported_image.get());
				m_detected_colours.clear();
			}
		}

		if (DrawPaletteSelector(m_selected_palette_index, m_available_palettes))
		{
			update_preview = true;
		}
		m_selected_palette_index = std::clamp(m_selected_palette_index, 0, static_cast<int>(m_available_palettes.size()) - 1);

		ImGui::SameLine();
		DrawPaletteSwatchPreview(*m_available_palettes.at(m_selected_palette_index));
		m_selected_palette = *m_available_palettes.at(m_selected_palette_index);

		if (m_imported_image == nullptr)
		{
			return;
		}
		ImGui::Text("Loaded: %s", m_loaded_path.c_str());

		if (m_rendered_imported_image == nullptr)
		{
			ImGui::Text("Rendering...");
			return;
		}

		ImGui::BeginGroup();
		{
			ImGui::Image((ImTextureID)m_rendered_imported_image.get(), { static_cast<float>(m_imported_image->w) * 2, static_cast<float>(m_imported_image->h) * 2 });

			if (ImGui::Button("Attempt to match colours"))
			{
				m_detected_colours.clear();
			}

			if (ImGui::Button("Force Update Preview"))
			{
				update_preview = true;
			}

			ImGui::Text("Palette colour mapping");
			ImGui::BeginDisabled();
			ImGui::Text("Original Colour -> Palette Colour Index");
			ImGui::EndDisabled();
			if (m_detected_colours.empty())
			{
				const SDL_PixelFormatDetails* pixel_format_details = SDL_GetPixelFormatDetails(m_imported_image->format);
				for (size_t i = 0; i < m_imported_image->w * m_imported_image->h; ++i)
				{
					const size_t byte_offset = pixel_format_details->bytes_per_pixel * i;
					const Uint32 packed_pixel = *reinterpret_cast<Uint32*>(&static_cast<Uint8*>(m_imported_image->pixels)[byte_offset]);
					const ImColor pixel
					{ static_cast<int>((packed_pixel & pixel_format_details->Rmask) >> pixel_format_details->Rshift)
					, static_cast<int>((packed_pixel & pixel_format_details->Gmask) >> pixel_format_details->Gshift)
					, static_cast<int>((packed_pixel & pixel_format_details->Bmask) >> pixel_format_details->Bshift)
					, static_cast<int>((packed_pixel & pixel_format_details->Amask) >> pixel_format_details->Ashift) };

					if (std::none_of(std::begin(m_detected_colours), std::end(m_detected_colours),
						[&pixel](const ColourPaletteMapping& colour_entry)
						{
							if (pixel.Value.w == 0)
							{
								return colour_entry.colour.Value.w == pixel.Value.w;;
							}

							return colour_entry.colour.Value.x == pixel.Value.x
								&& colour_entry.colour.Value.y == pixel.Value.y
								&& colour_entry.colour.Value.z == pixel.Value.z
								&& colour_entry.colour.Value.w == pixel.Value.w;
						}))
					{
						const auto& swatches = m_selected_palette.palette_swatches;
						const Uint16 packed_pixel_value = rom::Swatch::Pack(pixel.Value.x, pixel.Value.y, pixel.Value.z);

						const auto matched_colour = std::find_if(std::begin(swatches), std::end(swatches),
							[&packed_pixel_value](const rom::Swatch& swatch)
							{
								return swatch.packed_value == packed_pixel_value;
							});

						const Uint8 mapped_palette_index = (matched_colour != std::end(swatches)) ? static_cast<Uint8>(std::distance(std::begin(swatches), matched_colour)) : 0;
						m_detected_colours.emplace_back(ColourPaletteMapping{ pixel, mapped_palette_index });
					}
				}

				if (m_detected_colours.empty())
				{
					ImGui::EndGroup();
					return;
				}

				update_preview = true;
			}
			int i = 0;
			for (ColourPaletteMapping& colour_entry : m_detected_colours)
			{
				char id_buffer[64];
				sprintf_s(id_buffer, "col_%d", i);
				ImGui::ColorButton(id_buffer, colour_entry.colour);
				ImGui::SameLine();
				ImGui::Text("->");
				ImGui::SameLine();
				const rom::Swatch& swatch = m_selected_palette.palette_swatches[colour_entry.palette_index];
				ImColor col = swatch.AsImColor();
				ImGui::ColorButton(id_buffer, col.Value);

				sprintf_s(id_buffer, "###pal_sel_%d", i);
				int out_val = colour_entry.palette_index;
				ImGui::SameLine();
				ImGui::SetNextItemWidth(64);
				if (ImGui::InputInt(id_buffer, &out_val))
				{
					colour_entry.palette_index = static_cast<Uint8>(out_val) % 16;
					update_preview = true;
				}
				if (colour_entry.colour.Value.w == 0)
				{
					ImGui::SameLine();
					ImGui::Text("(Transparent)");
				}
				++i;
			}
		}
		ImGui::EndGroup();

		ImGui::SameLine();

		ImGui::BeginGroup();
		{
			if (update_preview)
			{
				m_preview_image = SDLSurfaceHandle{ SDL_CreateSurface(m_imported_image->w, m_imported_image->h, SDL_PIXELFORMAT_INDEX8) };
				m_preview_palette = Renderer::CreateSDLPalette(m_selected_palette);
				SDL_SetSurfacePalette(m_preview_image.get(), m_preview_palette.get());
				const SDL_PixelFormatDetails* import_pixel_format_details = SDL_GetPixelFormatDetails(m_imported_image->format);
				const SDL_PixelFormatDetails* preview_pixel_format_details = SDL_GetPixelFormatDetails(m_preview_image->format);

				const size_t preview_pitch_offset_per_line = m_preview_image->pitch - (m_preview_image->w * preview_pixel_format_details->bytes_per_pixel);
				size_t preview_pitch_offset = 0;

				for (size_t i = 0; i < m_preview_image->w * m_preview_image->h; ++i)
				{
					if (i != 0 && i % m_preview_image->w == 0)
					{
						preview_pitch_offset += preview_pitch_offset_per_line;
					}

					const size_t import_byte_offset = import_pixel_format_details->bytes_per_pixel * i;
					const Uint32 packed_pixel = static_cast<Uint32*>(m_imported_image->pixels)[i];
					const ImColor pixel
					{ static_cast<int>((packed_pixel & import_pixel_format_details->Rmask) >> import_pixel_format_details->Rshift)
					, static_cast<int>((packed_pixel & import_pixel_format_details->Gmask) >> import_pixel_format_details->Gshift)
					, static_cast<int>((packed_pixel & import_pixel_format_details->Bmask) >> import_pixel_format_details->Bshift)
					, static_cast<int>((packed_pixel & import_pixel_format_details->Amask) >> import_pixel_format_details->Ashift) };

					auto result_entry = std::find_if(std::begin(m_detected_colours), std::end(m_detected_colours),
						[&pixel](const ColourPaletteMapping& colour_entry)
						{
							if (pixel.Value.w == 0)
							{
								return colour_entry.colour.Value.w == pixel.Value.w;;
							}
							return colour_entry.colour.Value.x == pixel.Value.x
								&& colour_entry.colour.Value.y == pixel.Value.y
								&& colour_entry.colour.Value.z == pixel.Value.z
								&& colour_entry.colour.Value.w == pixel.Value.w;
						});

					if (result_entry != std::end(m_detected_colours))
					{
						static_cast<Uint8*>(m_preview_image->pixels)[i + preview_pitch_offset] = result_entry->palette_index;
					}
					else
					{
						static_cast<Uint8*>(m_preview_image->pixels)[i + preview_pitch_offset] = 0;
					}

				}

				m_rendered_preview_image = Renderer::RenderToTexture(m_preview_image.get());
				m_force_update_write_location = true;
			}
			else
			{
				if (m_rendered_preview_image == nullptr)
				{
					ImGui::Text("Rendering...");
					ImGui::EndGroup();
					return;
				}
			}

			if (m_rendered_preview_image != nullptr)
			{
				ImGui::Image((ImTextureID)m_rendered_preview_image.get(), { static_cast<float>(m_preview_image->w) * img_scale, static_cast<float>(m_preview_image->h) * img_scale });

				if (std::holds_alternative<rom::Sprite*>(m_target_asset))
				{
					DrawSpriteImport();
				}
				else if (std::holds_alternative<rom::TileSet*>(m_target_asset))
				{
					DrawTileSetImport();
				}
			}
		}
		ImGui::EndGroup();
	}

	static constexpr Uint32 picker_width = 20;

	void EditorImageImporter::RenderTileset(rom::TileSet& tileset)
	{
		m_export_preview_image = tileset.RenderToSurface(m_selected_palette);
		m_export_preview_texture = Renderer::RenderToTexture(m_export_preview_image.get());
	}

	void EditorImageImporter::DrawTileSetImport()
	{
		rom::TileSet& target_tileset = *std::get<rom::TileSet*>(m_target_asset);
		rom::TileSet* result_tileset = std::get<std::unique_ptr<rom::TileSet>>(m_result_asset).get();
		ImGui::SetNextItemWidth(256);
		int target_write_location = static_cast<int>(target_tileset.rom_data.rom_offset);
		m_num_tiles_to_insert = (m_preview_image->h / rom::TileSet::s_tile_height) * (m_preview_image->w / rom::TileSet::s_tile_width);
		const bool offset_changed = ImGui::InputInt("Target Write Offset", &target_write_location, 1, 100, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_ReadOnly);
		const bool append_mode_changed = ImGui::Checkbox("Append exiting tileset", &m_append_existing);
		bool tile_count_changed = ImGui::InputInt("Num Tiles", &m_num_tiles_to_insert, 0, 0xFF, ImGuiInputTextFlags_CharsHexadecimal);
		if (tile_count_changed || append_mode_changed || offset_changed || m_force_update_write_location)
		{
			m_force_update_write_location = false;
			m_result_asset = rom::TileSet::LoadFromROM(m_owning_ui.GetROM(), target_write_location, CompressionAlgorithm::SSC).tileset;
			result_tileset = std::get<std::unique_ptr<rom::TileSet>>(m_result_asset).get();

			if (m_append_existing == false)
			{
				result_tileset->uncompressed_data.clear();
				result_tileset->tiles.clear();
			}

			if (result_tileset != nullptr)
			{
				if( m_preview_image != nullptr)
				{
					const SDL_PixelFormatDetails* preview_pixel_format_details = SDL_GetPixelFormatDetails(m_preview_image->format);
					const size_t preview_pitch_offset_per_line = m_preview_image->pitch - (m_preview_image->w * preview_pixel_format_details->bytes_per_pixel);
					
					size_t preview_pitch_offset = 0;
					int tiles_to_add = m_num_tiles_to_insert == 0 ? (m_preview_image->h / rom::TileSet::s_tile_height) * (m_preview_image->w / rom::TileSet::s_tile_width) : m_num_tiles_to_insert;
					const size_t total_pixels = rom::TileSet::s_tile_width * rom::TileSet::s_tile_height;
					for(int y = 0; y < (m_preview_image->h / rom::TileSet::s_tile_height) && tiles_to_add > 0; ++y)
					{
						for (int x = 0; x < (m_preview_image->w / rom::TileSet::s_tile_width) && tiles_to_add > 0; ++x)
						{
							rom::Tile& new_tile = result_tileset->tiles.emplace_back();
							new_tile.pixel_data.resize(total_pixels);
							Uint8* current_byte = new_tile.pixel_data.data();

							int x_off = x * rom::TileSet::s_tile_width;
							int y_off = y * rom::TileSet::s_tile_height;
							int x_max = x_off + rom::TileSet::s_tile_width;
							int y_max = y_off + rom::TileSet::s_tile_height;

							size_t pixels_written = 0;
							size_t pixel_source_idx = (y_off * m_preview_image->pitch) + x_off;
							while (pixels_written < total_pixels && pixel_source_idx < m_preview_image->pitch * m_preview_image->h)
							{
								if (pixels_written != 0 && (pixels_written % rom::TileSet::s_tile_width) == 0)
								{
									pixel_source_idx = (y_off * m_preview_image->pitch) + (m_preview_image->pitch * (pixels_written / rom::TileSet::s_tile_width)) + x_off;
								}
								*current_byte = ((static_cast<Uint8*>(m_preview_image->pixels)[pixel_source_idx] & 0x0F) << 4);
								++pixel_source_idx;
								++pixels_written;

								if (pixels_written != 0 && (pixels_written % rom::TileSet::s_tile_width) == 0)
								{
									pixel_source_idx = (y_off * m_preview_image->pitch) + (m_preview_image->pitch * (pixels_written / rom::TileSet::s_tile_width)) + x_off;
								}
								*current_byte = *current_byte | static_cast<Uint8*>(m_preview_image->pixels)[pixel_source_idx] & 0x0F;
								++pixel_source_idx;
								++pixels_written;
								result_tileset->uncompressed_data.emplace_back(*current_byte);
								++current_byte;
							}

							--tiles_to_add;
						}
					}

					result_tileset->num_tiles = static_cast<Uint16>(result_tileset->tiles.size());
					result_tileset->uncompressed_size = static_cast<Uint16>(result_tileset->uncompressed_data.size());
				}


				RenderTileset(result_tileset != nullptr ? *result_tileset : target_tileset);
			}
		}

		if (result_tileset != nullptr)
		{
			ImGui::Image((ImTextureID)m_export_preview_texture.get(),
				ImVec2{ static_cast<float>(m_export_preview_texture->w) * img_scale, static_cast<float>(m_export_preview_texture->h) * img_scale },
				ImVec2{ 0,0 }, ImVec2{ static_cast<float>(m_export_preview_image->w) / m_export_preview_texture->w, static_cast<float>(m_export_preview_image->h) / m_export_preview_texture->h });

			if (ImGui::Button("/!\\ OVERWRITE TILESET IN ROM DATA /!\\"))
			{
				*std::get<rom::TileSet*>(m_target_asset) = std::move(*std::get<std::unique_ptr<rom::TileSet>>(m_result_asset).release());
			}
		}
	}

	void EditorImageImporter::DrawSpriteImport()
	{
		rom::Sprite& target_sprite = *std::get<rom::Sprite*>(m_target_asset);
		std::shared_ptr<const rom::Sprite> result_sprite = std::get<std::shared_ptr<const rom::Sprite>>(m_result_asset);
		ImGui::SetNextItemWidth(256);
		int target_write_location = static_cast<int>(target_sprite.rom_data.rom_offset);
		if (ImGui::InputInt("Target Write Offset", &target_write_location, 1, 100, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_ReadOnly) || m_force_update_write_location)
		{
			m_force_update_write_location = false;
			m_result_asset = rom::Sprite::LoadFromROM(m_owning_ui.GetROM(), target_write_location);
			result_sprite = std::get<std::shared_ptr<const rom::Sprite>>(m_result_asset);
			if(result_sprite != nullptr)
			{
				m_export_preview_image = SDLSurfaceHandle{ SDL_CreateSurface(result_sprite->GetBoundingBox().Width(), result_sprite->GetBoundingBox().Height(), SDL_PIXELFORMAT_INDEX8) };
				SDL_SetSurfacePalette(m_export_preview_image.get(), m_preview_palette.get());
				Renderer::SetPalette(m_preview_palette);
				result_sprite->RenderToSurface(m_export_preview_image.get());
				m_export_preview_texture = Renderer::RenderToTexture(m_export_preview_image.get());
			}
		}

		if (result_sprite != nullptr)
		{
			ImGui::Image((ImTextureID)m_export_preview_texture.get()
				, ImVec2(static_cast<float>(result_sprite->GetBoundingBox().Width()) * img_scale, static_cast<float>(result_sprite->GetBoundingBox().Height()) * img_scale));


			if (ImGui::Button("/!\\ OVERWRITE SPRITE IN ROM DATA /!\\"))
			{
				rom::Sprite& target_sprite = *std::get<rom::Sprite*>(m_target_asset);

				const BoundingBox bounds = result_sprite->GetBoundingBox();
				rom::SpinballROM& rom = m_owning_ui.GetROM();
				Uint8* current_byte = &rom.m_buffer[target_write_location];
				current_byte += 2; // tiles
				current_byte += 2; // vdp tiles

				for (const std::shared_ptr<rom::SpriteTile>& sprite_tile : result_sprite->sprite_tiles)
				{
					current_byte += 2; // xoffset
					current_byte += 2; // yoffset

					current_byte += 2; // ysize, xsize
				}

				for (const std::shared_ptr<rom::SpriteTile>& sprite_tile : result_sprite->sprite_tiles)
				{
					const SDL_PixelFormatDetails* preview_pixel_format_details = SDL_GetPixelFormatDetails(m_preview_image->format);
					const size_t preview_pitch_offset_per_line = m_preview_image->pitch - (m_preview_image->w * preview_pixel_format_details->bytes_per_pixel);
					size_t preview_pitch_offset = 0;


					const size_t total_pixels = sprite_tile->x_size * sprite_tile->y_size;
					if (total_pixels != 0)
					{
						int x_off = (sprite_tile->x_offset - bounds.min.x);
						int y_off = (sprite_tile->y_offset - bounds.min.y);
						int x_max = x_off + sprite_tile->x_size;
						int y_max = y_off + sprite_tile->y_size;

						size_t pixels_written = 0;
						size_t pixel_source_idx = (y_off * m_preview_image->pitch) + x_off;
						while (pixels_written < total_pixels && pixel_source_idx < m_preview_image->pitch * m_preview_image->h)
						{
							if (pixels_written != 0 && (pixels_written % sprite_tile->x_size) == 0)
							{
								pixel_source_idx = (y_off * m_preview_image->pitch) + (m_preview_image->pitch * (pixels_written / sprite_tile->x_size)) + x_off;
							}
							*current_byte = ((static_cast<Uint8*>(m_preview_image->pixels)[pixel_source_idx] & 0x0F) << 4);
							++pixel_source_idx;
							++pixels_written;

							if (pixels_written != 0 && (pixels_written % sprite_tile->x_size) == 0)
							{
								pixel_source_idx = (y_off * m_preview_image->pitch) + (m_preview_image->pitch * (pixels_written / sprite_tile->x_size)) + x_off;
							}
							*current_byte = *current_byte | static_cast<Uint8*>(m_preview_image->pixels)[pixel_source_idx] & 0x0F;
							++pixel_source_idx;
							++pixels_written;

							++current_byte;
						}
					}
				}
			}
		}
	}

	void EditorImageImporter::Update()
	{
		if (m_visible == false)
		{
			return;
		}

		ImGui::SetNextWindowPos(ImVec2{ 0,16 });
		ImGui::SetNextWindowSize(ImVec2{ Renderer::s_window_width, Renderer::s_window_height - 16 });
		if (ImGui::Begin("Image Importer", &m_visible, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings))
		{
			InnerUpdate();
		}
		ImGui::End();
	}

	void EditorImageImporter::SetTarget(rom::Sprite& target_sprite)
	{
		m_target_asset = &target_sprite;
		m_force_update_write_location = true;
	}

	void EditorImageImporter::SetTarget(rom::TileSet& target_tileset)
	{
		m_target_asset = &target_tileset;
		m_force_update_write_location = true;
	}

	void EditorImageImporter::SetAvailablePalettes(const std::vector<std::shared_ptr<rom::Palette>>& palette_lines)
	{
		m_available_palettes.clear();
		std::copy(std::begin(palette_lines), std::end(palette_lines), std::back_inserter(m_available_palettes));
	}

	void EditorImageImporter::SetAvailablePalettes(const rom::PaletteSetArray& palette_set_lines)
	{
		m_available_palettes.clear();
		std::copy(std::begin(palette_set_lines), std::end(palette_set_lines), std::back_inserter(m_available_palettes));
	}

}