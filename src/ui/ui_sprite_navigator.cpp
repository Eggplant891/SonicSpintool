#include "ui/ui_sprite_navigator.h"

#include "rom/spinball_rom.h"
#include "ui/ui_editor.h"

#include "imgui.h"
#include "SDL3/SDL_image.h"
#include "ui/ui_palette_viewer.h"
#include "rom/tile_layout.h"
#include "rom/tileset.h"
#include "types/rom_ptr.h"

#include <filesystem>
#include <optional>
#include <algorithm>
#include <thread>
#include <atomic>
#include <unordered_set>

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
			sprintf(search_ui_text, "Search for next (%d) sprites###search_button", num_searches);
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

			// SDL texture creation must happen on the main/render thread.  The old
			// implementation created textures from a detached worker thread; on Linux
			// this commonly returned null textures.  Changing the palette appeared to
			// "fix" the sprites because that forced them to be regenerated later on the
			// main thread.
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
						m_chosen_palette = std::clamp(m_chosen_palette, 0, static_cast<int>(palettes.size()) - 1);
						const rom::Palette& palette = *palettes[static_cast<size_t>(m_chosen_palette)];
						for (auto& sprite : ready_sprites)
						{
							if (!sprite)
								continue;
							sprite->texture = sprite->RenderTextureForPalette(palette);
							m_sprites_found.emplace_back(std::move(sprite));
						}
					}
				}
			}

			auto start_full_sprite_scan = [this]()
			{
				if (m_find_all_running.exchange(true))
				{
					return;
				}

				m_sprites_found.clear();
				{
					std::lock_guard<std::mutex> pending_lock(m_pending_sprites_mutex);
					m_pending_sprites.clear();
				}

				m_selected_sprite_rom_offset = 0;
				m_find_all_progress = 0.0f;

				std::thread([this]()
				{
					const size_t rom_size = m_owning_ui.GetROM().m_buffer.size();
					Uint32 working_offset = 0;

					while (working_offset < rom_size)
					{
						m_find_all_progress = rom_size == 0
							? 1.0f
							: working_offset / static_cast<float>(rom_size);

						auto sprite = rom::Sprite::LoadFromROM(
							m_owning_ui.GetROM(),
							working_offset
						);

						if (sprite)
						{
							const Uint32 sprite_end =
								sprite->rom_data.rom_offset_end;

							if (
								sprite_end > working_offset &&
								sprite_end <= rom_size
							)
							{
								std::lock_guard<std::mutex> pending_lock(
									m_pending_sprites_mutex
								);

								m_pending_sprites.emplace_back(
									std::make_shared<UISpriteTexture>(sprite)
								);
							}
						}

						// Test every byte. Do not jump to the end of a candidate:
						// a false positive could otherwise hide a valid sprite located
						// inside the skipped range. Odd ROM offsets are also valid
						// candidates for heuristic discovery.
						++working_offset;
					}

					m_find_all_progress = 1.0f;
					m_find_all_running = false;
				}).detach();
			};

			static bool automatic_full_scan_started = false;
			if (!automatic_full_scan_started && !m_owning_ui.GetROM().m_buffer.empty())
			{
				automatic_full_scan_started = true;
				start_full_sprite_scan();
			}

			if (ImGui::Button("Show all sprites"))
			{
				start_full_sprite_scan();
			}

			// SSC-compressed art is stored as a two-byte tile-count header
			// followed by an SSC stream.  Standard Sprite::LoadFromROM()
			// cannot see those graphics, so scan and render them as tilesets.
			static std::atomic<bool> ssc_scan_running{ false };
			static std::atomic<float> ssc_scan_progress{ 0.0f };
			static std::atomic<size_t> ssc_tilesets_found{ 0 };
			static std::mutex ssc_offsets_mutex;
			static std::unordered_set<Uint32> ssc_art_offsets;

			const bool any_scan_running =
				m_find_all_running.load() || ssc_scan_running.load();

			ImGui::BeginDisabled(any_scan_running);
			if (ImGui::Button("Show SSC compressed art"))
			{
				m_sprites_found.clear();
				{
					std::lock_guard<std::mutex> pending_lock(
						m_pending_sprites_mutex
					);
					m_pending_sprites.clear();
				}

				m_selected_sprite_rom_offset = 0;
				ssc_scan_progress = 0.0f;
				ssc_tilesets_found = 0;
				{
					std::lock_guard<std::mutex> offsets_lock(
						ssc_offsets_mutex
					);
					ssc_art_offsets.clear();
				}
				ssc_scan_running = true;

				std::thread([this]()
				{
					const rom::SpinballROM& rom = m_owning_ui.GetROM();
					const size_t rom_size = rom.m_buffer.size();
					size_t offset = 0;

					while (offset + 3 < rom_size)
					{
						ssc_scan_progress =
							rom_size == 0
								? 1.0f
								: static_cast<float>(offset) /
									static_cast<float>(rom_size);

						const Uint16 num_tiles = static_cast<Uint16>(
							(static_cast<Uint16>(rom.m_buffer[offset]) << 8U) |
							static_cast<Uint16>(rom.m_buffer[offset + 1])
						);

						// A practical upper bound reduces accidental decompression
						// attempts while still allowing very large art sheets.
						if (num_tiles == 0 || num_tiles > 0x1000)
						{
							++offset;
							continue;
						}

						TilesetEntry entry =
							rom::TileSet::LoadFromROM_SSCCompression(
								rom,
								static_cast<Uint32>(offset)
							);

						if (
							!entry.tileset ||
							entry.result.error_msg.has_value()
						)
						{
							++offset;
							continue;
						}

						const size_t expected_size =
							static_cast<size_t>(num_tiles) *
							rom::TileSet::s_tile_total_bytes;

						// Genuine SSC art blocks decode to exactly the tile count
						// announced by their two-byte header.
						if (
							entry.tileset->uncompressed_data.size() !=
								expected_size ||
							entry.tileset->tiles.size() != num_tiles
						)
						{
							++offset;
							continue;
						}

						std::shared_ptr<const rom::Sprite> sheet =
							entry.tileset->CreateSpriteFromTile(0);

						if (sheet)
						{
							std::lock_guard<std::mutex> pending_lock(
								m_pending_sprites_mutex
							);

							m_pending_sprites.emplace_back(
								std::make_shared<UISpriteTexture>(sheet)
							);

							{
								std::lock_guard<std::mutex> offsets_lock(
									ssc_offsets_mutex
								);
								ssc_art_offsets.insert(
									static_cast<Uint32>(offset)
								);
							}

							++ssc_tilesets_found;
						}

						const size_t next_offset =
							entry.tileset->rom_data.rom_offset_end;

						offset = next_offset > offset
							? next_offset
							: offset + 1;
					}

					ssc_scan_progress = 1.0f;
					ssc_scan_running = false;
				}).detach();
			}
			ImGui::EndDisabled();

			if (ssc_scan_running)
			{
				ImGui::ProgressBar(ssc_scan_progress.load());
				ImGui::TextDisabled(
					"Scanning the ROM for SSC-compressed VDP art..."
				);
			}

			if (ssc_tilesets_found.load() > 0)
			{
				ImGui::Text(
					"SSC art blocks: %zu",
					ssc_tilesets_found.load()
				);
			}

			if (m_find_all_running)
			{
				ImGui::ProgressBar(m_find_all_progress.load());
				ImGui::TextDisabled(
					"Scanning every ROM byte; large ROMs may take time."
				);
			}

			ImGui::Text(
				"Standard sprite candidates: %zu",
				m_sprites_found.size()
			);

			if (ImGui::Button("Clear Textures"))
			{
				m_sprites_found.clear();
				{
					std::lock_guard<std::mutex> pending_lock(m_pending_sprites_mutex);
					m_pending_sprites.clear();
				}
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

					sprintf(path_buffer, "popup_%X02",static_cast<unsigned int>(tex->sprite->rom_data.rom_offset));
					if (ImGui::BeginPopupContextItem(path_buffer, ImGuiPopupFlags_MouseButtonRight))
					{
						bool is_ssc_art = false;
						{
							std::lock_guard<std::mutex> offsets_lock(
								ssc_offsets_mutex
							);
							is_ssc_art =
								ssc_art_offsets.find(
									tex->sprite->rom_data.rom_offset
								) != ssc_art_offsets.end();
						}

						if (is_ssc_art)
						{
							ImGui::TextDisabled(
								"SSC compressed art (read-only)"
							);
						}
						else if (ImGui::MenuItem("Import image to this location"))
						{
							m_owning_ui.OpenImageImporter(
								const_cast<rom::Sprite&>(*tex->sprite)
							);
						}

						sprintf(path_buffer, "Export image at 0x%X02", static_cast<unsigned int>(tex->sprite->rom_data.rom_offset));
						if (ImGui::MenuItem(path_buffer))
						{
							sprintf(path_buffer, "spinball_image_%X02.png", static_cast<unsigned int>(tex->sprite->rom_data.rom_offset));
							std::filesystem::path export_path = m_owning_ui.GetSpriteExportPath().append(path_buffer);
							SDLPaletteHandle palette = Renderer::CreateSDLPalette(*m_owning_ui.GetPalettes().at(2));
							SDLSurfaceHandle out_surface{ SDL_CreateSurface(tex->sprite->GetBoundingBox().Width(), tex->sprite->GetBoundingBox().Height(), SDL_PIXELFORMAT_INDEX8)};
							SDL_SetSurfacePalette(out_surface.get(), palette.get());
							SDL_SetSurfaceColorKey(out_surface.get(), true, 0);
							tex->sprite->RenderToSurface(out_surface.get());
							assert(IMG_SavePNG(out_surface.get(), export_path.c_str()));
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
