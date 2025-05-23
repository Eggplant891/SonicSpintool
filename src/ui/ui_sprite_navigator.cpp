#include "ui/ui_sprite_navigator.h"

#include "rom/spinball_rom.h"
#include "ui/ui_editor.h"

#include "imgui.h"
#include <optional>
#include <algorithm>
#include "SDL3/SDL_image.h"
#include "ui/ui_palette_viewer.h"
#include <filesystem>
#include "rom/tile_layout.h"
#include "rom/tileset.h"
#include "types/rom_ptr.h"

namespace spintool
{
	void EditorSpriteNavigator::Update()
	{
		if (m_visible == false)
		{
			return;
		}

		if (m_show_arbitrary_render && m_random_texture != nullptr)
		{
			ImGui::SetNextWindowSize(ImVec2{ -1, -1 }, ImGuiCond_Always);
			if (ImGui::Begin("Arbitrary texture preview", &m_show_arbitrary_render, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing))
			{
				ImGui::Image((ImTextureID)m_random_texture.get(), { m_random_texture->w * m_zoom, m_random_texture->h * m_zoom });
			}
			ImGui::End();
		}
		if (ImGui::Begin("Sprite Discoverator", &m_visible))
		{
			static char path_buffer[4096];

			int working_offset = static_cast<int>(m_offset);
			if (ImGui::InputInt("ROM Offset", &working_offset, 1, 0x10, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue))
			{
				m_offset = *reinterpret_cast<unsigned int*>(&working_offset);
				m_attempt_render_of_arbitrary_data = true;
			}

			bool changed = false;
			if (ImGui::RadioButton("Render 8x8 VDP Tiles", m_render_arbitrary_with_palette == ArbitraryRenderMode::VDP_TILES))
			{
				m_render_arbitrary_with_palette = ArbitraryRenderMode::VDP_TILES;
				m_attempt_render_of_arbitrary_data = true;
			}
			ImGui::SameLine();
			if (ImGui::RadioButton("Render 16-bit Colour", m_render_arbitrary_with_palette == ArbitraryRenderMode::VDP_COLOUR))
			{
				m_render_arbitrary_with_palette = ArbitraryRenderMode::VDP_COLOUR;
				m_attempt_render_of_arbitrary_data = true;
			}
			ImGui::SameLine();
			if (ImGui::RadioButton("Render Palette Colour", m_render_arbitrary_with_palette == ArbitraryRenderMode::PALETTE))
			{
				m_render_arbitrary_with_palette = ArbitraryRenderMode::PALETTE;
				m_attempt_render_of_arbitrary_data = true;
			}
			ImGui::SameLine();
			if (ImGui::Checkbox("Display arbitrary render preview", &m_show_arbitrary_render))
			{
				m_attempt_render_of_arbitrary_data |= true;
				m_random_texture.reset();
			}
			if (m_render_arbitrary_with_palette == ArbitraryRenderMode::VDP_TILES)
			{
				ImGui::SetNextItemWidth(128);
				m_attempt_render_of_arbitrary_data |= ImGui::InputInt("Num Tiles Width", &m_arbitrary_num_tiles_width, 1, 10, ImGuiInputTextFlags_EnterReturnsTrue);
				ImGui::SameLine();
				ImGui::SetNextItemWidth(128);
				m_attempt_render_of_arbitrary_data |= ImGui::InputInt("Num Tiles Height", &m_arbitrary_num_tiles_height, 1, 10, ImGuiInputTextFlags_EnterReturnsTrue);
			}
			else
			{
				ImGui::SetNextItemWidth(128);
				m_attempt_render_of_arbitrary_data |= ImGui::InputInt("Width", &m_arbitrary_texture_width, 1, 10, ImGuiInputTextFlags_EnterReturnsTrue);
				ImGui::SameLine();
				ImGui::SetNextItemWidth(128);
				m_attempt_render_of_arbitrary_data |= ImGui::InputInt("Height", &m_arbitrary_texture_height, 1, 10, ImGuiInputTextFlags_EnterReturnsTrue);
			}
			if (m_attempt_render_of_arbitrary_data && m_show_arbitrary_render)
			{
				switch (m_render_arbitrary_with_palette)
				{
					case ArbitraryRenderMode::PALETTE:
					{
						m_random_texture = Renderer::RenderArbitaryOffsetToTexture(m_owning_ui.GetROM(), m_offset, { m_arbitrary_texture_width, m_arbitrary_texture_height }, *m_owning_ui.GetPalettes().at(m_chosen_palette));
					}
					break;

					case ArbitraryRenderMode::VDP_COLOUR:
					{
						m_random_texture = Renderer::RenderArbitaryOffsetToTexture(m_owning_ui.GetROM(), m_offset, { m_arbitrary_texture_width, m_arbitrary_texture_height });
					}
					break;

					case ArbitraryRenderMode::VDP_TILES:
					{
						m_random_texture = Renderer::RenderArbitaryOffsetToTilesetTexture(m_owning_ui.GetROM(), m_offset, { m_arbitrary_num_tiles_width, m_arbitrary_num_tiles_height });
					}
					break;
				}
				m_attempt_render_of_arbitrary_data = false;
			}

			static int num_searches = 16;
			char search_ui_text[64];
			sprintf_s(search_ui_text, "Search for next (%d) sprites###search_button", num_searches);
			if (ImGui::Button(search_ui_text))
			{
				const auto current_sprite = std::find_if(std::begin(m_sprites_found), std::end(m_sprites_found),
					[this](const std::shared_ptr<UISpriteTexture>& sprite_tex)
					{
						return sprite_tex != nullptr && sprite_tex->sprite->rom_data.rom_offset == m_offset;
					});
				const UISpriteTexture* next_sprite = nullptr;
				if (current_sprite != std::end(m_sprites_found))
				{
					next_sprite = (*current_sprite).get();
					m_offset += next_sprite->sprite->GetSizeOf();
				}

				for (size_t a = 0; a < num_searches && m_offset < m_owning_ui.GetROM().m_buffer.size(); ++a)
				{
					const auto found_sprite = std::find_if(std::begin(m_sprites_found), std::end(m_sprites_found),
						[this](const std::shared_ptr<UISpriteTexture>& sprite_tex)
						{
							return sprite_tex != nullptr && sprite_tex->sprite->rom_data.rom_offset == m_offset;
						});

					if (found_sprite == std::end(m_sprites_found))
					{
						std::shared_ptr<const rom::Sprite> new_sprite = rom::Sprite::LoadFromROM(m_owning_ui.GetROM(), m_offset);

						if (new_sprite != nullptr)
						{
							m_offset = new_sprite->rom_data.rom_offset_end;
							m_sprites_found.emplace_back(std::make_shared<UISpriteTexture>(new_sprite));
							m_sprites_found.back()->texture = m_sprites_found.back()->RenderTextureForPalette(*m_owning_ui.GetPalettes().at(m_chosen_palette));
						}
						else
						{
							m_offset += 2;
							--a;
						}
					}
				}
			}
			ImGui::SameLine();
			ImGui::SetNextItemWidth(128);
			ImGui::InputInt("###num_sprites_to_find", &num_searches, 1, 16, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue);

			if (ImGui::Button("Show Toxic Caves Sprites"))
			{
				//const static rom::Ptr32 toxic_caves_sprite_table = 0x12B0C;
				const static rom::Ptr32 toxic_caves_sprite_table = 0xE47A;
				//const static rom::Ptr32 toxic_caves_sprite_table = 0x318ee;
				const static rom::Ptr32 toxic_caves_sprite_table_begin = toxic_caves_sprite_table + 4;
				const Uint16 num_sprites = m_owning_ui.GetROM().ReadUint16(toxic_caves_sprite_table);
				std::vector<rom::Ptr32> sprite_offsets;
				sprite_offsets.reserve(num_sprites);

				for (Uint32 i = 0; i < num_sprites; ++i)
				{
					sprite_offsets.emplace_back(m_owning_ui.GetROM().ReadUint16(toxic_caves_sprite_table_begin + (i*2)));
				}

				for (rom::Ptr32 offset : sprite_offsets)
				{
					if (std::shared_ptr<const rom::Sprite> new_sprite = rom::Sprite::LoadFromROM(m_owning_ui.GetROM(), toxic_caves_sprite_table + offset))
					{
						m_sprites_found.emplace_back(std::make_shared<UISpriteTexture>(new_sprite));
						m_sprites_found.back()->texture = m_sprites_found.back()->RenderTextureForPalette(*m_owning_ui.GetPalettes().at(m_chosen_palette));
					}
				}
			}

			if (ImGui::Button("Show current sprite"))
			{
				const auto found_sprite = std::find_if(std::begin(m_sprites_found), std::end(m_sprites_found),
					[this](const std::shared_ptr<UISpriteTexture>& sprite_tex)
					{
						return sprite_tex != nullptr && sprite_tex->sprite->rom_data.rom_offset == m_offset;
					});

				if (found_sprite != std::end(m_sprites_found))
				{
					m_selected_sprite_rom_offset = (*found_sprite)->sprite->rom_data.rom_offset;
				}
				else
				{
					if (std::shared_ptr<const rom::Sprite> new_sprite = rom::Sprite::LoadFromROM(m_owning_ui.GetROM(), m_offset))
					{
						m_sprites_found.emplace_back(std::make_shared<UISpriteTexture>(new_sprite));
						m_sprites_found.back()->texture = m_sprites_found.back()->RenderTextureForPalette(*m_owning_ui.GetPalettes().at(m_chosen_palette));
					}
				}
			}

			if (ImGui::Button("Show Loaded Tilesets"))
			{
				for (const TilesetEntry& tileset_entry : m_owning_ui.GetTilesets())
				{
					const std::shared_ptr<const rom::TileSet>& tileset = tileset_entry.tileset;

					ImGui::SeparatorText("Tileset");
					Uint32 offset = 0;
					while (true)
					{
						if (tileset->rom_data.rom_offset_end <= offset)
						{
							break;
						}

						std::shared_ptr<const rom::Sprite> new_sprite = tileset->CreateSpriteFromTile(offset);
						if (new_sprite == nullptr)
						{
							break;
						}
						offset = new_sprite->rom_data.rom_offset_end;
						m_sprites_found.emplace_back(std::make_shared<UISpriteTexture>(new_sprite));
						m_sprites_found.back()->texture = m_sprites_found.back()->RenderTextureForPalette(*m_owning_ui.GetPalettes().at(m_chosen_palette));
					}
				}
			}

			if (ImGui::Button("Goto next sprite"))
			{
				const auto current_sprite = std::find_if(std::begin(m_sprites_found), std::end(m_sprites_found),
					[this](const std::shared_ptr<UISpriteTexture>& sprite_tex)
					{
						return sprite_tex->sprite->rom_data.rom_offset == m_offset;
					});

				if (current_sprite != std::end(m_sprites_found))
				{
					m_offset += (*current_sprite)->sprite->GetSizeOf();
					const auto found_sprite = std::find_if(std::begin(m_sprites_found), std::end(m_sprites_found),
						[this](const std::shared_ptr<UISpriteTexture>& sprite_tex)
						{
							return sprite_tex->sprite->rom_data.rom_offset == m_offset;
						});

					if (found_sprite == std::end(m_sprites_found))
					{
						if (std::shared_ptr<const rom::Sprite> new_sprite = rom::Sprite::LoadFromROM(m_owning_ui.GetROM(), m_offset))
						{
							m_sprites_found.emplace_back(std::make_shared<UISpriteTexture>(new_sprite));
							m_sprites_found.back()->texture = m_sprites_found.back()->RenderTextureForPalette(*m_owning_ui.GetPalettes().front());
						}
					}
				}
			}

			static std::atomic<bool> render_process_running = false;
			static std::atomic<float> render_process_progress = 0.0f;

			if (ImGui::Button("Find all sprites"))
			{
				m_sprites_found.clear();

				auto render_textures_thread = [&]()
					{
						std::vector<std::shared_ptr<UISpriteTexture>> results;

						render_process_running = true;
						render_process_progress = 0.0f;

						results.clear();

						Uint32 working_offset = 0;
						while (working_offset < m_owning_ui.GetROM().m_buffer.size())
						{
							render_process_progress = working_offset / static_cast<float>(m_owning_ui.GetROM().m_buffer.size());

							std::shared_ptr<const rom::Sprite> new_sprite = rom::Sprite::LoadFromROM(m_owning_ui.GetROM(), working_offset);
							if (new_sprite == nullptr)
							{
								working_offset+=2;
								continue;
							}
							results.emplace_back(std::make_shared<UISpriteTexture>(new_sprite));
							working_offset += new_sprite->GetSizeOf();
							new_sprite.reset();

						}

						render_process_progress = 0.0f;

						Uint32 progress = 0;
						for (Uint32 i = 0; i < results.size(); ++i)
						{
							constexpr Uint32 num_cycles_until_wait = 32;
							for (Uint32 cycles = 0; cycles < num_cycles_until_wait && i < results.size(); ++cycles, ++i)
							{
								std::shared_ptr<UISpriteTexture>& sprite = results[i];

								auto current_sprite_it = std::find_if(std::begin(results), std::end(results),
									[&sprite](const std::shared_ptr<UISpriteTexture>& spr)
									{
										return spr->sprite->rom_data.rom_offset == sprite->sprite->rom_data.rom_offset;
									});
								if (current_sprite_it != std::end(results))
								{
									m_owning_ui.m_render_to_texture_mutex.lock();
									std::shared_ptr<UISpriteTexture>& new_sprite = m_sprites_found.emplace_back(*current_sprite_it);
									new_sprite->texture = new_sprite->RenderTextureForPalette(*m_owning_ui.GetPalettes().at(m_chosen_palette));
									++progress;
									m_owning_ui.m_render_to_texture_mutex.unlock();

								}
								render_process_progress = progress / static_cast<float>(results.size());
							}
							SDL_Delay(0);
						}
						render_process_running = false;
						results.clear();

						return 0;
					};
				std::thread new_thread{ render_textures_thread };
				new_thread.detach();
			}

			if (render_process_running)
			{
				ImGui::ProgressBar(render_process_progress);
			}

			if (ImGui::Button("Clear Textures"))
			{
				m_sprites_found.clear();
				m_selected_sprite_rom_offset = 0;
			}

			if (DrawPaletteSelectorWithPreview(m_chosen_palette, m_owning_ui))
			{
				for (std::shared_ptr<UISpriteTexture>& tex : m_sprites_found)
				{
					tex->texture.reset();
				}
				m_attempt_render_of_arbitrary_data = true;
			}

			const ImVec2 previous_spacing = ImGui::GetStyle().ItemSpacing;

			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 2, 0 });
			ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2{ 2, 0 });

			ImGui::SliderFloat("Zoom", &m_zoom, 1.0f, 8.0f, "%.1f");

			if (ImGui::BeginChild("Image Results"))
			{
				const int cursor_start_x_pos = static_cast<int>(ImGui::GetCursorPosX());
				const int padding_x = static_cast<int>(ImGui::GetStyle().ItemSpacing.x);
				int current_width = cursor_start_x_pos;
				m_owning_ui.m_render_to_texture_mutex.lock();
				for (std::shared_ptr<UISpriteTexture>& tex : m_sprites_found)
				{
					bool started_newline = false;

					if (tex->texture == nullptr)
					{
						tex->texture = tex->RenderTextureForPalette(*m_owning_ui.GetPalettes().at(m_chosen_palette));
					}

					if (tex->dimensions.x == 0 || tex->dimensions.y == 0)
					{
						continue;
					}

					if (ImGui::GetContentRegionAvail().x < current_width + padding_x + (tex->dimensions.x * m_zoom))
					{
						started_newline = true;
						ImGui::NewLine();
						ImGui::SameLine(); ImGui::Dummy(ImVec2(0, 0));
						current_width = static_cast<int>(ImGui::GetCursorPosX());
					}

					if (started_newline == false)
					{
						ImGui::SameLine();
					}

					tex->DrawForImGui(m_zoom);
					current_width += static_cast<int>((tex->dimensions.x * m_zoom) + ImGui::GetStyle().ItemSpacing.x);

					const bool hovered = ImGui::IsItemHovered();
					const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

					sprintf_s(path_buffer, "popup_%X02",static_cast<unsigned int>(tex->sprite->rom_data.rom_offset));
					if (ImGui::BeginPopupContextItem(path_buffer, ImGuiPopupFlags_MouseButtonRight))
					{
						if (ImGui::MenuItem("Import image to this location"))
						{
							m_owning_ui.OpenSpriteImporter(static_cast<int>(tex->sprite->rom_data.rom_offset));
						}

						sprintf_s(path_buffer, "Export image at 0x%X02", static_cast<unsigned int>(tex->sprite->rom_data.rom_offset));
						if (ImGui::MenuItem(path_buffer))
						{
							sprintf_s(path_buffer, "spinball_image_%X02.png", static_cast<unsigned int>(tex->sprite->rom_data.rom_offset));
							std::filesystem::path export_path = m_owning_ui.GetSpriteExportPath().append(path_buffer);
							SDLPaletteHandle palette = Renderer::CreateSDLPalette(*m_owning_ui.GetPalettes().at(2));
							SDLSurfaceHandle out_surface{ SDL_CreateSurface(tex->sprite->GetBoundingBox().Width(), tex->sprite->GetBoundingBox().Height(), SDL_PIXELFORMAT_INDEX8)};
							SDL_SetSurfacePalette(out_surface.get(), palette.get());
							SDL_SetSurfaceColorKey(out_surface.get(), true, 0);
							tex->sprite->RenderToSurface(out_surface.get());
							assert(IMG_SavePNG(out_surface.get(), export_path.generic_u8string().c_str()));
						}
						ImGui::EndPopup();
					}

					if (m_selected_sprite_rom_offset == tex->sprite->rom_data.rom_offset)
					{
						ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImGui::GetColorU32(ImVec4{ 0,192,0,255 }), 1.0f, 0, 2);
					}
					if (hovered)
					{
						ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImGui::GetColorU32(ImVec4{ 255,255,255,255 }), 1.0f, 0, 2);
					}
					if (clicked)
					{
						m_selected_sprite_rom_offset = tex->sprite->rom_data.rom_offset;
						m_owning_ui.OpenSpriteViewer(tex->sprite);
					}
				}
				m_owning_ui.m_render_to_texture_mutex.unlock();
			}
			ImGui::EndChild();
			ImGui::PopStyleVar(2);
		}
		ImGui::End();
	}
}