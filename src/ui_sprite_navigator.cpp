#include "ui_sprite_navigator.h"

#include "spinball_rom.h"
#include "ui_editor.h"

#include "imgui.h"
#include <optional>
#include <algorithm>

namespace spintool
{

	EditorSpriteNavigator::EditorSpriteNavigator(EditorUI& owning_ui)
		: m_owning_ui(owning_ui)
		, m_rom(owning_ui.GetROM())
		, m_palette_viewer(owning_ui)
	{

	}

	void EditorSpriteNavigator::InitialisePalettes(const size_t num_palettes)
	{
		constexpr size_t offset = 0xDFC;
		unsigned char* current_byte = &m_owning_ui.GetROM().m_buffer[offset];
		m_palettes.clear();

		for (size_t p = 0; p < num_palettes; ++p)
		{
			VDPPalette palette;
			palette.offset = current_byte - m_rom.m_buffer.data();

			for (size_t i = 0; i < 16; ++i)
			{
				const Uint8 first_byte = *current_byte;
				++current_byte;
				const Uint8 second_byte = *current_byte;
				++current_byte;
				VDPSwatch swatch;
				swatch.packed_value = static_cast<Uint16>((static_cast<Uint16>(first_byte) << 8) | second_byte);

				palette.palette_swatches[i] = swatch;
			}
			m_palettes.emplace_back(palette);
		}
	}

	void EditorSpriteNavigator::Update()
	{
		m_palette_viewer.Update(m_palettes);

		if (ImGui::Begin("Sprite Navigator"))
		{
			static char path_buffer[4096];
			if (ImGui::InputText("Path", path_buffer, sizeof(path_buffer), ImGuiInputTextFlags_EnterReturnsTrue))
			{
				m_rom.LoadROMFromPath(path_buffer);
				std::optional<SpinballSprite> result = m_rom.LoadSprite(m_offset, m_use_packed_data_mode, m_read_between_sprites);
				if (result)
				{
					m_sprites_found.emplace_back(*result);
					m_sprites_found.back().texture = m_sprites_found.back().RenderTextureForPalette(m_palettes.front());
				}
			}

			ImGui::Checkbox("Packed pixel data (2 per byte)", &m_use_packed_data_mode);
			ImGui::Checkbox("Read data between sprites", &m_read_between_sprites);
			if (ImGui::InputInt("ROM Offset", &m_offset, 1, 0x10, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue))
			{
				
			}

			static int num_searches = 16;
			char search_ui_text[64];
			sprintf_s(search_ui_text, "Search for next (%d) sprites###search_button", num_searches);
			if (ImGui::Button(search_ui_text))
			{
				for (size_t a = 0; a < num_searches && m_offset < m_rom.m_buffer.size();)
				{
					for (size_t i = m_offset + 4; i < m_rom.m_buffer.size(); ++i)
					{
						if (m_rom.m_buffer[i] == 0xFF || m_rom.m_buffer[i + 2] == 0xFF)
						{
							m_offset = static_cast<int>(i);
							std::optional<SpinballSprite> new_sprite = m_rom.LoadSprite(m_offset, m_use_packed_data_mode, m_read_between_sprites);

							if (new_sprite && new_sprite->x_size > 0 && new_sprite->x_size <= 320 && new_sprite->y_size > 0 && new_sprite->y_size <= 240)
							{
								m_sprites_found.emplace_back(std::move(*new_sprite));
								m_sprites_found.back().texture = m_sprites_found.back().RenderTextureForPalette(m_palettes.front());
								++a;
								break;
							}
							else
							{
								continue;
							}
						}
					}
				}
			}
			ImGui::SameLine();
			ImGui::SetNextItemWidth(128);
			ImGui::InputInt("###num_sprites_to_find", &num_searches, 1, 16, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue);

			if (ImGui::Button("Show current sprite"))
			{
				const auto found_sprite = std::find_if(std::begin(m_sprites_found), std::end(m_sprites_found),
					[this](const UISpriteTexture& sprite_tex)
					{
						return sprite_tex.sprite.rom_offset == m_offset;
					});

				if (found_sprite != std::end(m_sprites_found))
				{
					m_selected_sprite_rom_offset = found_sprite->sprite.rom_offset;
				}
				else
				{
					std::optional<SpinballSprite> new_sprite = m_rom.LoadSprite(m_offset, m_use_packed_data_mode, m_read_between_sprites);
					m_sprites_found.emplace_back(std::move(*new_sprite));
					m_sprites_found.back().texture = m_sprites_found.back().RenderTextureForPalette(m_palettes.front());
				}
			}

			if (ImGui::Button("Goto next sprite"))
			{
				for (size_t i = m_offset + 4; i < m_rom.m_buffer.size(); ++i)
				{
					if (m_rom.m_buffer[i] == 0xFF || m_rom.m_buffer[i+2] == 0xFF)
					{
						m_offset = static_cast<int>(i);
						std::optional<SpinballSprite> new_sprite = m_rom.LoadSprite(m_offset, m_use_packed_data_mode, m_read_between_sprites);

						if (new_sprite && new_sprite->x_size > 0 && new_sprite->x_size <= 320 && new_sprite->y_size > 0 && new_sprite->y_size <= 240)
						{
							m_sprites_found.emplace_back(std::move(*new_sprite));
							m_sprites_found.back().texture = m_sprites_found.back().RenderTextureForPalette(m_palettes.front());
						}
						else
						{
							continue;
						}
						break;
					}
				}
				//offset += static_cast<int>(current_sprite.real_size);
			}

			if (ImGui::TreeNode("Selected Sprite Info"))
			{
				if (m_selected_sprite_rom_offset != 0)
				{
					auto selected_sprite_it = std::find_if(std::begin(m_sprites_found), std::end(m_sprites_found), [this](const UISpriteTexture& spr)
						{
							return m_selected_sprite_rom_offset = spr.sprite.rom_offset;
						});

					SpinballSprite* selected_sprite = nullptr;
					if (selected_sprite_it != std::end(m_sprites_found))
					{
						selected_sprite = &selected_sprite_it->sprite;
					}

					ImGui::Text("Offset: X: %d (%02X) Y: %d (%02X)", selected_sprite->x_offset, selected_sprite->x_offset, selected_sprite->y_offset, selected_sprite->y_offset);

					ImGui::Text("X Size: %d", selected_sprite->x_size);
					ImGui::SameLine();
					ImGui::Text("Y Size: %d", selected_sprite->y_size);
					int working_val[2] = { selected_sprite->x_size, selected_sprite->y_size };
					if (ImGui::InputInt2("Change Size", working_val, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue))
					{
						selected_sprite->x_size = static_cast<Uint8>(working_val[0]);
						selected_sprite->y_size = static_cast<Uint8>(working_val[1]);
						//RenderForPalettesAndEmplace(selected_sprite);
					}
					ImGui::Text("Total Pixels: %d (%d)", selected_sprite->pixel_data.size(), selected_sprite->x_size * selected_sprite->y_size);
					ImGui::Text("Real Size: %04X", selected_sprite->real_size);
					ImGui::Text("Unrealised Size: %04X", selected_sprite->unrealised_size);
				}
				else
				{
					ImGui::BeginDisabled();
					ImGui::Text("No Sprite Selected");
					ImGui::EndDisabled();
				}
				ImGui::TreePop();
			}

			static std::atomic<bool> render_process_running = false;
			static std::atomic<float> render_process_progress = 0.0f;

			if (ImGui::Button("Find all sprites"))
			{
				m_sprites_found.clear();

				auto render_textures_thread = [&]()
					{
						std::vector<UISpriteTexture> results;

						render_process_running = true;
						render_process_progress = 0.0f;

						results.clear();

						size_t working_offset = 0;
						while (working_offset < m_rom.m_buffer.size())
							//for (size_t a = 0; a < 1024; ++a)
						{
							render_process_progress = working_offset / static_cast<float>(m_rom.m_buffer.size());
							//auto next_offset = m_rom.FindOffsetForNextSprite(working_offset);

							//working_offset = static_cast<int>(next_offset.value());
							std::optional<SpinballSprite> new_sprite = m_rom.LoadSprite(working_offset, m_use_packed_data_mode, m_read_between_sprites);
							if (new_sprite.has_value() == false)
							{
								working_offset+=2;
								continue;
							}
							results.emplace_back(std::move(*new_sprite));
							//working_offset += new_sprite->real_size;
							working_offset+=2;

						}

						render_process_progress = 0.0f;

						size_t progress = 0;
						for (size_t i = 0; i < results.size(); ++i)
						{
							constexpr size_t num_cycles_until_wait = 32;
							for (size_t cycles = 0; cycles < num_cycles_until_wait && i < results.size(); ++cycles, ++i)
							{
								UISpriteTexture& sprite = results[i];

								if (sprite.sprite.rom_offset > 0xC0000)
								{
									break;
								}

								auto current_sprite_it = std::find_if(std::begin(results), std::end(results),
									[&sprite](const UISpriteTexture& spr)
									{
										return spr.sprite.rom_offset == sprite.sprite.rom_offset;
									});
								if (current_sprite_it != std::end(results))
								{
									m_owning_ui.m_render_to_texture_mutex.lock();
									UISpriteTexture& new_sprite = m_sprites_found.emplace_back(current_sprite_it->sprite);
									new_sprite.texture = current_sprite_it->RenderTextureForPalette(m_palettes.at(m_chosen_palette));
									++progress;
									m_owning_ui.m_render_to_texture_mutex.unlock();

								}
								render_process_progress = progress / static_cast<float>(results.size());
							}
							SDL_Delay(0);
						}
						render_process_running = false;
						results.clear();
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

			ImGui::SetNextItemWidth(256);
			if (ImGui::SliderInt("###Palette Index", &m_chosen_palette, 0, static_cast<int>(m_palettes.size()-1)))
			{
				for (UISpriteTexture& sprite : m_sprites_found)
				{
					sprite.texture = sprite.RenderTextureForPalette(m_palettes.at(m_chosen_palette));
				}
			}
			ImGui::SameLine();
			EditorPaletteViewer::RenderPalette(m_palettes.at(m_chosen_palette), m_chosen_palette);

			const ImVec2 previous_spacing = ImGui::GetStyle().ItemSpacing;

			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 2, 0 });
			ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2{ 2, 0 });

			if (ImGui::BeginChild("Image Results"))
			{
				const int max_width = static_cast<int>(ImGui::GetWindowWidth());
				int current_width = 0;
				m_owning_ui.m_render_to_texture_mutex.lock();
				for (UISpriteTexture& tex : m_sprites_found)
				{
					bool started_newline = false;
					if (tex.texture == nullptr || tex.dimensions.x == 0 || tex.dimensions.y == 0)
					{
						continue;
					}

					if (current_width + tex.dimensions.x > max_width)
					{
						started_newline = true;
						ImGui::NewLine();
						current_width = 0;
						ImGui::SameLine(); ImGui::Dummy(ImVec2(0, 0));
					}

					if (started_newline == false)
					{
						ImGui::SameLine();
					}

					ImGui::Image((ImTextureID)tex.texture.get(), ImVec2(static_cast<float>(tex.dimensions.x), static_cast<float>(tex.dimensions.y)), { 0,0 }, { static_cast<float>(tex.dimensions.x) / tex.texture->w, static_cast<float>((tex.dimensions.y) / tex.texture->h) });
					current_width += static_cast<int>(tex.dimensions.x + ImGui::GetStyle().ItemSpacing.x);

					if (m_selected_sprite_rom_offset == tex.sprite.rom_offset)
					{
						ImGui::GetForegroundDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImGui::GetColorU32(ImVec4{ 0,192,0,255 }), 1.0f, 0, 2);
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::GetForegroundDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImGui::GetColorU32(ImVec4{ 255,255,255,255 }), 1.0f, 0, 2);
					}
					if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
					{
						m_selected_sprite_rom_offset = tex.sprite.rom_offset;
						m_owning_ui.OpenSpriteViewer(tex.sprite, m_palettes);
					}
				}
				m_owning_ui.m_render_to_texture_mutex.unlock();
			}
			ImGui::EndChild();
			ImGui::PopStyleVar(2);
		}
		ImGui::End();
	}

	void EditorSpriteNavigator::SetPalettes(std::vector<VDPPalette>& palettes)
	{
		m_palettes.reserve(palettes.size());
		for (const VDPPalette& palette : palettes)
		{
			m_palettes.emplace_back(palette);
		}
	}

}