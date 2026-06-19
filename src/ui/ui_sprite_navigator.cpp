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
#include <limits>
#include <cstdio>

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
			static Uint32 sprite_scan_end_offset = 0x03909D;

			const size_t rom_size_bytes =
				m_owning_ui.GetROM().m_buffer.size();

			const Uint32 maximum_rom_offset =
				rom_size_bytes == 0
					? 0
					: static_cast<Uint32>(
						std::min<size_t>(
							rom_size_bytes - 1,
							std::numeric_limits<Uint32>::max()
						)
					);

			sprite_scan_end_offset = std::min(
				sprite_scan_end_offset,
				maximum_rom_offset
			);

			ImGui::SetNextItemWidth(150.0f);
			ImGui::InputScalar(
				"Sprite scan end",
				ImGuiDataType_U32,
				&sprite_scan_end_offset,
				nullptr,
				nullptr,
				"%06X",
				ImGuiInputTextFlags_CharsHexadecimal
			);

			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip(
					"Last ROM offset included in automatic and manual sprite scans."
				);
			}

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

				for (
					size_t a = 0;
					a < num_searches &&
					m_offset <= sprite_scan_end_offset &&
					m_offset < m_owning_ui.GetROM().m_buffer.size();
					++a
				)
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

			const Uint32 rom_size = static_cast<Uint32>(
				std::min<size_t>(
					m_owning_ui.GetROM().m_buffer.size(),
					std::numeric_limits<Uint32>::max()
				)
			);

			if (m_scan_end_offset == 0 || m_scan_end_offset > rom_size)
			{
				m_scan_end_offset = rom_size;
			}

			int scan_start = static_cast<int>(m_scan_start_offset);
			int scan_end = static_cast<int>(m_scan_end_offset);

			ImGui::SetNextItemWidth(150.0f);
			if (ImGui::InputInt(
				"Scan start##sprite_scan",
				&scan_start,
				2,
				0x100,
				ImGuiInputTextFlags_CharsHexadecimal
			))
			{
				m_scan_start_offset = static_cast<Uint32>(
					std::max(scan_start, 0)
				);
			}

			ImGui::SameLine();
			ImGui::SetNextItemWidth(150.0f);
			if (ImGui::InputInt(
				"Scan end##sprite_scan",
				&scan_end,
				2,
				0x100,
				ImGuiInputTextFlags_CharsHexadecimal
			))
			{
				m_scan_end_offset = static_cast<Uint32>(
					std::max(scan_end, 0)
				);
			}

			ImGui::SameLine();
			ImGui::SetNextItemWidth(120.0f);
			ImGui::InputInt(
				"Maximum results##sprite_scan",
				&m_scan_max_results,
				100,
				500
			);
			m_scan_max_results = std::clamp(m_scan_max_results, 1, 20000);

			if (
				ImGui::Button("Scan sprite candidates") &&
				!m_find_all_running.exchange(true)
			)
			{
				m_sprites_found.clear();
				{
					std::lock_guard<std::mutex> pending_lock(
						m_pending_sprites_mutex
					);
					m_pending_sprites.clear();
				}

				const Uint32 scan_begin = std::min(
					m_scan_start_offset,
					rom_size
				) & ~Uint32{1};
				const Uint32 scan_finish = std::clamp(
					m_scan_end_offset,
					scan_begin,
					rom_size
				);
				const Uint32 maximum_results = static_cast<Uint32>(
					m_scan_max_results
				);

				m_find_all_progress = 0.0f;
				m_find_all_result_count = 0;
				m_find_all_cancel = false;

				std::thread([
					this,
					scan_begin,
					scan_finish,
					maximum_results
				]()
				{
					Uint32 working_offset = scan_begin;
					const Uint32 scan_length = scan_finish - scan_begin;

					while (
						working_offset + 4 <= scan_finish &&
						!m_find_all_cancel.load() &&
						m_find_all_result_count.load() < maximum_results
					)
					{
						m_find_all_progress = scan_length == 0
							? 1.0f
							: (working_offset - scan_begin) /
								static_cast<float>(scan_length);

						auto sprite = rom::Sprite::LoadFromROM(
							m_owning_ui.GetROM(),
							working_offset
						);

						if (sprite)
						{
							const BoundingBox bounds = sprite->GetBoundingBox();
							const bool plausible_dimensions =
								bounds.Width() > 0 &&
								bounds.Height() > 0 &&
								bounds.Width() <= 512 &&
								bounds.Height() <= 512;

							if (plausible_dimensions)
							{
								std::lock_guard<std::mutex> pending_lock(
									m_pending_sprites_mutex
								);
								m_pending_sprites.emplace_back(
									std::make_shared<UISpriteTexture>(sprite)
								);
								++m_find_all_result_count;
							}
						}

						working_offset += 2;
					}

					m_find_all_progress = 1.0f;
					m_find_all_running = false;
				}).detach();
			}

			if (m_find_all_running)
			{
				ImGui::SameLine();
				if (ImGui::Button("Cancel scan"))
				{
					m_find_all_cancel = true;
				}

				char progress_overlay[96];
				std::snprintf(
					progress_overlay,
					sizeof(progress_overlay),
					"%u candidate(s)",
					m_find_all_result_count.load()
				);
				ImGui::ProgressBar(
					m_find_all_progress.load(),
					ImVec2{-1.0f, 0.0f},
					progress_overlay
				);
			}
			else if (m_find_all_result_count.load() > 0)
			{
				ImGui::Text(
					"Scan complete: %u candidate(s)",
					m_find_all_result_count.load()
				);
			}

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
					if (ImGui::IsItemHovered())
					{
						ImGui::BeginTooltip();
						ImGui::Text(
							"ROM offset: 0x%06X",
							static_cast<unsigned int>(
								tex->sprite->rom_data.rom_offset
							)
						);
						ImGui::Text(
							"Tiles: %u | VDP tiles: %u",
							tex->sprite->num_tiles,
							tex->sprite->num_vdp_tiles
						);
						ImGui::Text(
							"Size: %.0f x %.0f px",
							tex->dimensions.x,
							tex->dimensions.y
						);
						ImGui::EndTooltip();
					}
					current_width += static_cast<int>((tex->dimensions.x * m_zoom) + ImGui::GetStyle().ItemSpacing.x);

					const bool hovered = ImGui::IsItemHovered();
					const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

					sprintf(path_buffer, "popup_%X02",static_cast<unsigned int>(tex->sprite->rom_data.rom_offset));
					if (ImGui::BeginPopupContextItem(path_buffer, ImGuiPopupFlags_MouseButtonRight))
					{
						if (ImGui::MenuItem("Import image to this location"))
						{
							m_owning_ui.OpenImageImporter(const_cast<rom::Sprite&>(*tex->sprite));
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
