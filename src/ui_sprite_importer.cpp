#include "ui_sprite_importer.h"

#include "imgui.h"
#include "SDL3/SDL_image.h"
#include <algorithm>
#include "spinball_rom.h"
#include "ui_editor.h"

namespace spintool
{
	EditorSpriteImporter::EditorSpriteImporter(EditorUI& owning_ui)
		: m_owning_ui(owning_ui)
	{
	}

	void EditorSpriteImporter::InnerUpdate()
	{
		m_selected_palette = m_owning_ui.GetROM().LoadPalettes(8).at(2);

		static char path_buffer[4096] = "E:\\Development\\_RETRODEV\\MegaDrive\\Spinball\\_ASSETS\\tails_spinball\\tails_idle.png";;
		
		if (ImGui::InputText("Path", path_buffer, sizeof(path_buffer), ImGuiInputTextFlags_EnterReturnsTrue))
		{
			SDLSurfaceHandle loaded_surface{ IMG_Load(path_buffer) };
			if (loaded_surface != nullptr)
			{
				m_loaded_path = path_buffer;
				m_imported_image = SDLSurfaceHandle{ SDL_ConvertSurface(loaded_surface.get(), SDL_PIXELFORMAT_RGBA32) };
				m_rendered_imported_image = Renderer::RenderToTexture(m_imported_image.get());
			}
		}

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

		ImGui::Image((ImTextureID)m_rendered_imported_image.get(), { static_cast<float>(m_imported_image->w) * 2, static_cast<float>(m_imported_image->h) * 2 });

		ImGui::Text("Detected Colours");
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
					m_detected_colours.emplace_back(ColourPaletteMapping{ pixel, 0 });
				}
			}

			if (m_detected_colours.empty())
			{
				ImGui::EndGroup();
				return;
			}
		}

		bool update_preview = false;
		if (ImGui::Button("Update Preview"))
		{
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
			const VDPSwatch& swatch = m_selected_palette.palette_swatches[colour_entry.palette_index];
			ImColor col = swatch.AsImColor();
			ImGui::ColorButton(id_buffer, col.Value);

			sprintf_s(id_buffer, "###pal_sel_%d", i);
			int out_val = colour_entry.palette_index;
			ImGui::SameLine();
			ImGui::SetNextItemWidth(128);
			if (ImGui::InputInt(id_buffer, &out_val))
			{
				colour_entry.palette_index = static_cast<Uint8>(out_val);
				update_preview = true;
			}
			++i;
		}

		ImGui::EndGroup();

		ImGui::SameLine();

		ImGui::BeginGroup();
		if (update_preview)
		{
			m_preview_image = SDLSurfaceHandle{ SDL_CreateSurface(m_imported_image->w, m_imported_image->h, SDL_PIXELFORMAT_INDEX8) };
			m_preview_palette = Renderer::CreatePalette(m_selected_palette);
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
					static_cast<Uint8*>(m_preview_image->pixels)[i+ preview_pitch_offset] = result_entry->palette_index;
				}
				else
				{
					static_cast<Uint8*>(m_preview_image->pixels)[i+ preview_pitch_offset] = 0;
				}

			}

			m_rendered_preview_image = Renderer::RenderToTexture(m_preview_image.get());
		}
		else
		{
			if (m_rendered_preview_image == nullptr)
			{
				ImGui::Text("Rendering...");
				ImGui::EndGroup();
				return;
			}

			ImGui::Image((ImTextureID)m_rendered_preview_image.get(), { static_cast<float>(m_preview_image->w) * 2, static_cast<float>(m_preview_image->h) * 2 });
		}

		if (m_rendered_preview_image != nullptr)
		{
			if (ImGui::InputInt("Target Write Offset", &m_target_write_location))
			{
				if (m_result_sprite = m_owning_ui.GetROM().LoadSprite(m_target_write_location, true, false))
				{
					m_export_preview_image = SDLSurfaceHandle{ SDL_CreateSurface(m_result_sprite->GetBoundingBox().Width(), m_result_sprite->GetBoundingBox().Height(), SDL_PIXELFORMAT_INDEX8) };
					SDL_SetSurfacePalette(m_export_preview_image.get(), m_preview_palette.get());
					m_result_sprite->RenderToSurface(m_export_preview_image.get());
					m_export_preview_texture = Renderer::RenderToTexture(m_export_preview_image.get());
				}
			}

			if (m_result_sprite != nullptr)
			{
				constexpr float img_scale = 2.0f;
				ImGui::Image((ImTextureID)m_export_preview_texture.get()
					, ImVec2(static_cast<float>(m_result_sprite->GetBoundingBox().Width()), static_cast<float>(m_result_sprite->GetBoundingBox().Height())));

				if (ImGui::Button("/!\\ OVERWRITE SPRITE IN ROM DATA /!\\"))
				{
					assert(m_result_sprite->sprite_tiles.at(0)->x_size == m_preview_image->w && m_result_sprite->sprite_tiles.at(0)->y_size == m_preview_image->h);
					SpinballROM& rom = m_owning_ui.GetROM();
					unsigned char* current_byte = &rom.m_buffer[m_target_write_location];
					current_byte += 2; // tiles
					current_byte += 2; // vdp tiles

					current_byte += 2; // xoffset
					current_byte += 2; // yoffset

					current_byte += 2; // ysize, xsize

					const SDL_PixelFormatDetails* preview_pixel_format_details = SDL_GetPixelFormatDetails(m_preview_image->format);
					const size_t preview_pitch_offset_per_line = m_preview_image->pitch - (m_preview_image->w * preview_pixel_format_details->bytes_per_pixel);
					size_t preview_pitch_offset = 0;

					for (size_t i = 0; i < m_preview_image->w * m_preview_image->h; i += 2)
					{
						if (i != 0 && i % m_preview_image->w == 0)
						{
							preview_pitch_offset += preview_pitch_offset_per_line;
						}

						*current_byte = ((static_cast<Uint8*>(m_preview_image->pixels)[i] & 0x0F) << 4) | (static_cast<Uint8*>(m_preview_image->pixels)[i+1] & 0x0F);
						++current_byte;
					}
				}
			}
		}

		ImGui::EndGroup();
	}

	void EditorSpriteImporter::Update()
	{
		if (visible == false)
		{
			return;
		}

		ImGui::SetNextWindowSize({ 1024, -1 });
		if (ImGui::Begin("Sprite Importer", &visible))
		{
			InnerUpdate();
		}
		ImGui::End();
	}
}