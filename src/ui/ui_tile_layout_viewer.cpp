#include "ui/ui_tile_layout_viewer.h"

#include "ui/ui_editor.h"

#include "rom/sprite.h"
#include "rom/tileset.h"
#include "rom/tile_layout.h"
#include "rom/culling_tables/game_obj_collision_culling_table.h"
#include "rom/culling_tables/animated_object_culling_table.h"
#include "rom/culling_tables/spline_culling_table.h"

#include "SDL3/SDL_image.h"
#include "imgui.h"
#include <memory>
#include <cassert>
#include <limits>
#include <algorithm>
#include <iterator>
#include <cmath>
#include <iostream>
#include "imgui_internal.h"
#include <cstdio>
#include "rom/rom_asset_definitions.h"
#include "editor/game_obj_manager.h"
#include <numeric>
#include "rom/level.h"


namespace spintool
{
	EditorTileLayoutViewer::EditorTileLayoutViewer(EditorUI& owning_ui)
		: EditorWindowBase(owning_ui)
	{
	}

	RenderRequestType EditorTileLayoutViewer::DrawMenuBar()
	{
		RenderRequestType out_render_request = RenderRequestType::NONE;
		if (m_render_from_edit)
		{
			out_render_request = RenderRequestType::LEVEL;
		}

		const int previous_level_index = m_level != nullptr ? m_level->m_level_index : -1;
		int level_index = -1;

		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				ImGui::BeginDisabled(m_level == nullptr || m_level->m_level_index == -1);

				if (ImGui::Selectable("Repaint Level"))
				{
					m_render_from_edit = true;
					out_render_request = RenderRequestType::LEVEL;
				}

				if (ImGui::Selectable("Reload Level"))
				{
					level_index = previous_level_index;
					out_render_request = RenderRequestType::LEVEL;
				}

				if (ImGui::Selectable("Save Level"))
				{
					if (m_level != nullptr)
					{
						m_level->m_spline_culling_table = m_spline_manager.GenerateSplineCullingTable();
						m_level->m_game_object_culling_table = m_game_object_manager.GenerateObjCollisionCullingTable();
						m_level->m_anim_object_culling_table = m_game_object_manager.GenerateAnimObjCullingTable();
						m_level->m_game_obj_instances.clear();

						for (const std::unique_ptr<UIGameObject>& game_obj : m_game_object_manager.game_objects)
						{
							m_level->m_game_obj_instances.emplace_back(game_obj->obj_definition);
						}

						m_level->SaveToROM(m_owning_ui.GetROM());
					}
				}

				ImGui::EndDisabled();
				ImGui::Separator();


				ImGui::Checkbox("Export", &m_export_result);

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Load"))
			{
				if (ImGui::BeginMenu("Level"))
				{
					if (ImGui::Selectable("Toxic Caves"))
					{
						out_render_request = RenderRequestType::LEVEL;
						level_index = 0;
					}

					if (ImGui::Selectable("Lava Powerhouse"))
					{
						out_render_request = RenderRequestType::LEVEL;
						level_index = 1;
					}

					if (ImGui::Selectable("The Machine"))
					{
						out_render_request = RenderRequestType::LEVEL;
						level_index = 2;
					}

					if (ImGui::Selectable("Showdown"))
					{
						out_render_request = RenderRequestType::LEVEL;
						level_index = 3;
					}

					if (level_index != -1 && level_index == previous_level_index)
					{
						m_render_from_edit = true;
					}

					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Intro"))
				{
					if (ImGui::Selectable("Background"))
					{
						out_render_request = RenderRequestType::INTRO;
					}
					if (ImGui::Selectable("Foreground"))
					{
						out_render_request = RenderRequestType::INTRO;
					}
					if (ImGui::Selectable("Robotnik Ship"))
					{
						out_render_request = RenderRequestType::INTRO_SHIP;
					}
					if (ImGui::Selectable("Water"))
					{
						out_render_request = RenderRequestType::INTRO_WATER;
					}
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Frontend"))
				{
					if (ImGui::Selectable("Background"))
					{
						//preview_menu_bg = true;
					}
					if (ImGui::Selectable("Foreground"))
					{
						//preview_menu_fg = true;
					}
					if (ImGui::Selectable("Combined"))
					{
						out_render_request = RenderRequestType::MENU;
					}
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Bonus"))
				{
					if (ImGui::Selectable("Background"))
					{
						//preview_bonus_bg = true;
					}
					if (ImGui::Selectable("Foreground"))
					{
						//preview_bonus_fg = true;
					}
					if (ImGui::Selectable("Combined"))
					{
						out_render_request = RenderRequestType::BONUS;
					}
					ImGui::Separator();
					ImGui::Checkbox("Alt palette", &m_preview_bonus_alt_palette);
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Other"))
				{
					if (ImGui::Selectable("Preview Sega Logo"))
					{
						out_render_request = RenderRequestType::SEGA_LOGO;
					}

					if (ImGui::Selectable("Preview Options Menu"))
					{
						out_render_request = RenderRequestType::OPTIONS;
					}
					ImGui::EndMenu();
				}

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("View"))
			{
				ImGui::SeparatorText("Layers");
				ImGui::Checkbox("Collision", &m_layer_settings.collision);
				ImGui::SeparatorText("Culling Visualisation");
				if (ImGui::Checkbox("Spline Culling Sectors", &m_layer_settings.spline_culling))
				{
					if (m_layer_settings.spline_culling)
					{
						m_working_culling_table = *m_spline_manager.GenerateSplineCullingTable();
					}
				}
				ImGui::Checkbox("Collision Culling Sectors", &m_layer_settings.collision_culling);
				ImGui::Checkbox("Visibility Culling Sectors", &m_layer_settings.visibility_culling);
				ImGui::SeparatorText("Tooltips");
				ImGui::Checkbox("Game Objects", &m_layer_settings.hover_game_objects);
				ImGui::Checkbox("Game Object Tooltips", &m_layer_settings.hover_game_objects_tooltip);
				ImGui::Checkbox("Spline Collision Info", &m_layer_settings.hover_splines);
				ImGui::Checkbox("Radial Collision Info", &m_layer_settings.hover_radials);
				ImGui::Checkbox("Tile Info", &m_layer_settings.hover_brushes);
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Debug"))
			{
				if (ImGui::Selectable("Test Collision Culling"))
				{
					TestCollisionCullingResults();
				}
				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}

		if (level_index != -1)
		{
			m_level = std::make_shared<rom::Level>(rom::Level::LoadFromROM(m_owning_ui.GetROM(), level_index));
			m_level->m_tile_layers.emplace_back();
			m_level->m_tile_layers.emplace_back();
			m_selected_brush.Clear();
			m_selected_tile.Clear();
			m_working_brush.reset();
			m_working_flipper.reset();
			m_spline_manager.LoadFromSplineCullingTable(rom::SplineCullingTable::LoadFromROM(m_owning_ui.GetROM(), m_owning_ui.GetROM().ReadUint32(m_level->m_data_offsets.collision_data_terrain)));
		}

		return out_render_request;
	}

	void EditorTileLayoutViewer::Update()
	{
		if (IsOpen() == false)
		{
			return;
		}

		if (m_brush_editor && m_brush_editor->IsOpen())
		{
			m_brush_editor->Update();
			return;
		}
		else if (m_brush_editor)
		{
			m_brush_editor.reset();
			m_working_brush.reset();
			m_working_layer_index.reset();
		}

		ImGui::SetNextWindowPos(ImVec2{ 0,16 });
		ImGui::SetNextWindowSize(ImVec2{ Renderer::s_window_width, Renderer::s_window_height - 16 });
		if (ImGui::Begin("Tile Layout Viewer", &m_visible, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings))
		{
			bool render_flippers = false;
			bool render_rings = false;

			RenderRequestType render_request = DrawMenuBar();
			if (render_request != RenderRequestType::NONE)
			{
				PrepareRenderRequest(render_request);
			}

			if (m_popup_msg)
			{
				if (ImGui::IsPopupOpen(m_popup_msg->title.c_str()) == false)
				{
					ImGui::OpenPopup(m_popup_msg->title.c_str());
				}
				bool is_open = m_popup_msg.has_value();
				if (ImGui::BeginPopupModal(m_popup_msg->title.c_str(), &is_open, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar))
				{
					ImGui::Text(m_popup_msg->body.c_str());
					if (ImGui::Button("Dismiss"))
					{
						m_popup_msg.reset();
						ImGui::CloseCurrentPopup();
					}
					ImGui::EndPopup();
				}

				if (is_open == false)
				{
					m_popup_msg.reset();
				}
			}

			if (render_request == RenderRequestType::LEVEL)
			{
				render_flippers = true;
				render_rings = true;
			}

			if (render_flippers)
			{
				const rom::AnimObjectDefinition flipper_def = rom::AnimObjectDefinition::LoadFromROM(m_owning_ui.GetROM(), m_owning_ui.GetROM().ReadUint32(m_level->m_data_offsets.flipper_object_definition));
				AnimSpriteEntry entry;
				entry.sprite_table = m_owning_ui.GetROM().ReadUint32(flipper_def.sprite_table);
				entry.anim_sequence = rom::AnimationSequence::LoadFromROM(m_owning_ui.GetROM(), flipper_def.starting_animation, entry.sprite_table);

				const auto& command_sequence = entry.anim_sequence->command_sequence;
				const auto& first_frame_with_sprite = std::find_if(std::begin(command_sequence), std::end(command_sequence), [](const rom::AnimationCommand& command)
					{
						return command.ui_frame_sprite != nullptr;
					});

				if ((m_flipper_preview.sprite == nullptr || m_render_from_edit == false) && first_frame_with_sprite != std::end(command_sequence))
				{
					m_flipper_preview.sprite = SDLSurfaceHandle{ SDL_CreateSurface(44, 31, SDL_PIXELFORMAT_INDEX8) };
					m_flipper_preview.palette = Renderer::CreateSDLPalette(*m_working_palette_set.palette_lines[flipper_def.palette_index % 4]);
					SDL_SetSurfacePalette(m_flipper_preview.sprite.get(), m_flipper_preview.palette.get());
					SDL_FillSurfaceRect(m_game_object_preview.sprite.get(), nullptr, 0);
					SDL_SetSurfaceColorKey(m_flipper_preview.sprite.get(), true, 0);
					first_frame_with_sprite->ui_frame_sprite->sprite->RenderToSurface(m_flipper_preview.sprite.get());
					m_flipper_preview.texture = Renderer::RenderToTexture(m_flipper_preview.sprite.get());
				}
				const Uint32 flippers_table_begin = m_owning_ui.GetROM().ReadUint32(m_level->m_data_offsets.flipper_data);
				const Uint32 num_flippers = m_owning_ui.GetROM().ReadUint16(m_level->m_data_offsets.flipper_count);

				m_level->m_flipper_instances.clear();
				m_level->m_flipper_instances.reserve(num_flippers);

				Uint32 current_table_offset = flippers_table_begin;

				for (Uint32 i = 0; i < num_flippers; ++i)
				{
					m_level->m_flipper_instances.emplace_back(rom::FlipperInstance::LoadFromROM(m_owning_ui.GetROM(), current_table_offset));
					current_table_offset = m_level->m_flipper_instances.back().rom_data.rom_offset_end;
				}
			}

			if (render_rings)
			{
				if (m_ring_preview.sprite == nullptr || m_render_from_edit == false)
				{
					const static Uint32 ring_sprite_offset = 0x0000F6D8;

					std::shared_ptr<const rom::Sprite> ring_sprite = rom::Sprite::LoadFromROM(m_owning_ui.GetROM(), ring_sprite_offset);
					m_ring_preview.sprite = SDLSurfaceHandle{ SDL_CreateSurface(ring_sprite->GetBoundingBox().Width(), ring_sprite->GetBoundingBox().Height(), SDL_PIXELFORMAT_INDEX8) };
					m_ring_preview.palette = Renderer::CreateSDLPalette(*m_working_palette_set.palette_lines[3].get());
					SDL_SetSurfacePalette(m_ring_preview.sprite.get(), m_ring_preview.palette.get());
					SDL_FillSurfaceRect(m_game_object_preview.sprite.get(), nullptr, 0);
					SDL_SetSurfaceColorKey(m_ring_preview.sprite.get(), true, 0);
					ring_sprite->RenderToSurface(m_ring_preview.sprite.get());
					m_ring_preview.texture = Renderer::RenderToTexture(m_ring_preview.sprite.get());
				}

				/////////////

				m_game_object_preview.sprite = SDLSurfaceHandle{ SDL_CreateSurface(32, 32, SDL_PIXELFORMAT_RGBA32) };
				SDL_ClearSurface(m_game_object_preview.sprite.get(), 255, 255, 255, 255);
				SDL_SetSurfaceColorKey(m_game_object_preview.sprite.get(), true, SDL_MapRGBA(SDL_GetPixelFormatDetails(m_game_object_preview.sprite->format), nullptr, 255, 0, 0, 255));

				m_game_object_manager.game_objects.clear();
				m_anim_sprite_instances.clear();
				m_level->m_game_obj_instances.clear();
				m_level->m_game_obj_instances.reserve(m_level->m_data_offsets.object_instances.count);

				{
					Uint32 current_table_offset = m_level->m_data_offsets.object_instances.offset;

					for (Uint32 i = 0; i < m_level->m_data_offsets.object_instances.count; ++i)
					{
						{
							auto new_def = rom::GameObjectDefinition::LoadFromROM(m_owning_ui.GetROM(), current_table_offset);
							m_level->m_game_obj_instances.emplace_back(std::move(new_def));
							if (new_def.instance_id == 0)
							{
								current_table_offset = m_level->m_game_obj_instances.back().rom_data.rom_offset_end;
								continue;
							}
						}

						const rom::Ptr32 anim_def_offset = m_level->m_game_obj_instances.back().animation_definition;
						rom::AnimObjectDefinition anim_obj = rom::AnimObjectDefinition::LoadFromROM(m_owning_ui.GetROM(), anim_def_offset);

						auto found_sprite = std::find_if(std::begin(m_anim_sprite_instances), std::end(m_anim_sprite_instances), [&anim_obj](const AnimSpriteEntry& entry)
							{
								return anim_obj.sprite_table == entry.sprite_table && anim_obj.starting_animation == entry.anim_sequence->rom_data.rom_offset;
							});

						if (found_sprite == std::end(m_anim_sprite_instances))
						{
							AnimSpriteEntry entry;
							entry.sprite_table = m_owning_ui.GetROM().ReadUint32(anim_obj.sprite_table);
							entry.anim_sequence = rom::AnimationSequence::LoadFromROM(m_owning_ui.GetROM(), anim_obj.starting_animation, entry.sprite_table);

							const auto& command_sequence = entry.anim_sequence->command_sequence;
							const auto& first_frame_with_sprite = std::find_if(std::begin(command_sequence), std::end(command_sequence), [](const rom::AnimationCommand& command)
								{
									return command.ui_frame_sprite != nullptr;
								});

							if (first_frame_with_sprite != std::end(command_sequence))
							{
								entry.anim_command_used_for_surface = std::distance(std::begin(command_sequence), first_frame_with_sprite);
								const auto& first_frame_sprite = first_frame_with_sprite->ui_frame_sprite->sprite;
								auto new_sprite_surface = SDLSurfaceHandle{ SDL_CreateSurface(first_frame_sprite->GetBoundingBox().Width(), first_frame_sprite->GetBoundingBox().Height(), SDL_PIXELFORMAT_INDEX8) };
								entry.palette = Renderer::CreateSDLPalette(*m_working_palette_set.palette_lines.at(anim_obj.palette_index % 4).get());
								SDL_SetSurfacePalette(new_sprite_surface.get(), entry.palette.get());
								SDL_FillSurfaceRect(new_sprite_surface.get(), nullptr, 0);
								SDL_SetSurfaceColorKey(new_sprite_surface.get(), true, 0);
								first_frame_sprite->RenderToSurface(new_sprite_surface.get());

								entry.sprite_surface = std::move(new_sprite_surface);
							}


							m_anim_sprite_instances.emplace_back(std::move(entry));
						}
						current_table_offset = m_level->m_game_obj_instances.back().rom_data.rom_offset_end;
					}
				}
			}

			const bool will_be_rendering_preview = m_tile_layout_render_requests.empty() == false;
			const bool export_combined = m_tile_layout_render_requests.size() > 1;
			static std::optional<RenderTileLayoutRequest> current_preview_data;
			if (will_be_rendering_preview && export_combined == false)
			{
				current_preview_data = m_tile_layout_render_requests.front();
			}

			char combined_buffer[128];
			if (export_combined)
			{
				sprintf_s(combined_buffer, "%s_combined", m_tile_layout_render_requests.front().layout_type_name.c_str());
			}
			const std::string combined_layout_name = export_combined ? m_tile_layout_render_requests.front().layout_layout_name : "";
			const std::string combined_type_name = export_combined ? combined_buffer : "";
			SDLSurfaceHandle layout_preview_bg_surface;
			SDLSurfaceHandle layout_preview_fg_surface;
			if (will_be_rendering_preview)
			{
				bool will_require_mirror = false;
				int largest_width = std::numeric_limits<int>::min();
				int largest_height = std::numeric_limits<int>::min();

				if (m_render_from_edit == false)
				{
					m_tileset_preview_list.clear();
					m_tile_picker_list.clear();
				}
				m_render_from_edit = false;

				for (const auto& request : m_tile_layout_render_requests)
				{
					largest_width = std::max<int>(request.tile_layout_width * request.tile_brush_width, largest_width);
					largest_height = std::max<int>(request.tile_layout_height * request.tile_brush_height, largest_height);
					will_require_mirror |= request.draw_mirrored_layout;
				}

				if (will_require_mirror)
				{
					largest_width *= 2;
				}

				const RenderTileLayoutRequest& request = m_tile_layout_render_requests.front();
				layout_preview_bg_surface = SDLSurfaceHandle{ SDL_CreateSurface(rom::TileSet::s_tile_width * largest_width, rom::TileSet::s_tile_height * largest_height, SDL_PIXELFORMAT_RGBA32) };
				SDL_ClearSurface(layout_preview_bg_surface.get(), 0.0f, 0.0f, 0.0f, 0.0f);

				layout_preview_fg_surface = SDLSurfaceHandle{ SDL_CreateSurface(rom::TileSet::s_tile_width * largest_width, rom::TileSet::s_tile_height * largest_height, SDL_PIXELFORMAT_RGBA32) };
				SDL_ClearSurface(layout_preview_fg_surface.get(), 0.0f, 0.0f, 0.0f, 0.0f);
			}

			while (m_tile_layout_render_requests.empty() == false)
			{
				bool preserve_rendered_items = false;
				const RenderTileLayoutRequest& request = m_tile_layout_render_requests.front();

				if (request.store_tileset != nullptr && *request.store_tileset != nullptr)
				{
					m_working_tileset = *request.store_tileset;
					preserve_rendered_items = true;
				}
				else
				{
					if (request.compression_algorithm == CompressionAlgorithm::SSC)
					{
						m_working_tileset = rom::TileSet::LoadFromROM(m_owning_ui.GetROM(), request.tileset_address).tileset;
					}
					else if (request.compression_algorithm == CompressionAlgorithm::LZSS)
					{
						m_working_tileset = rom::TileSet::LoadFromROM_LZSSCompression(m_owning_ui.GetROM(), request.tileset_address).tileset;
					}

					if (request.store_tileset != nullptr)
					{
						*request.store_tileset = m_working_tileset;
					}
				}

				if (request.store_layout != nullptr && *request.store_layout != nullptr)
				{
					m_working_tile_layout = *request.store_layout;
					preserve_rendered_items = true;
				}
				else
				{
					if (request.compression_algorithm == CompressionAlgorithm::SSC)
					{
						m_working_tile_layout = rom::TileLayout::LoadFromROM(m_owning_ui.GetROM(), request.tile_layout_width, request.tile_brushes_address, request.tile_brushes_address_end, request.tile_layout_address, request.tile_layout_address_end);
					}
					else if (request.compression_algorithm == CompressionAlgorithm::LZSS)
					{
						m_working_tile_layout = rom::TileLayout::LoadFromROM(m_owning_ui.GetROM(), *m_working_tileset, request.tile_layout_address, request.tile_layout_address_end);
					}

					if (request.store_layout != nullptr)
					{
						*request.store_layout = m_working_tile_layout;
					}
				}

				m_working_tile_layout->layout_width = request.tile_layout_width;
				m_working_tile_layout->layout_height = request.tile_layout_height;

				if (export_combined == false)
				{
					current_preview_data->tile_layout_address = m_working_tile_layout->rom_data.rom_offset;
					current_preview_data->tile_layout_address_end = m_working_tile_layout->rom_data.rom_offset_end;
				}

				if (preserve_rendered_items == false)
				{
					std::vector<rom::Sprite> brushes;
					m_tileset_preview_list.emplace_back();
					auto& tile_picker = m_tile_picker_list.emplace_back(m_owning_ui);
					auto& brush_previews = m_tileset_preview_list.back().brushes;
					brushes.reserve(m_working_tile_layout->tile_brushes.size());
					brush_previews.clear();
					brush_previews.reserve(brushes.capacity());

					for (size_t brush_index = 0; brush_index < m_working_tile_layout->tile_brushes.size(); ++brush_index)
					{
						rom::TileBrushBase& brush = *m_working_tile_layout->tile_brushes[brush_index].get();
						rom::Sprite& brush_sprite = brushes.emplace_back();
						for (size_t i = 0; i < brush.tiles.size(); ++i)
						{
							rom::TileInstance& tile = brush.tiles[i];
							std::shared_ptr<rom::SpriteTile> sprite_tile = m_working_tileset->CreateSpriteTileFromTile(tile.tile_index);

							if (sprite_tile == nullptr)
							{
								break;
							}

							const size_t current_brush_offset = i;

							sprite_tile->x_offset = static_cast<Sint16>(current_brush_offset % brush.BrushWidth()) * rom::TileSet::s_tile_width;
							sprite_tile->y_offset = static_cast<Sint16>((current_brush_offset - (current_brush_offset % brush.BrushWidth())) / brush.BrushWidth()) * rom::TileSet::s_tile_height;

							sprite_tile->blit_settings.flip_horizontal = tile.is_flipped_horizontally;
							sprite_tile->blit_settings.flip_vertical = tile.is_flipped_vertically;

							sprite_tile->blit_settings.palette = tile.palette_line == 0 && request.palette_line.has_value() ? m_working_palette_set.palette_lines.at(*request.palette_line) : m_working_palette_set.palette_lines.at(tile.palette_line);
							brush_sprite.sprite_tiles.emplace_back(std::move(sprite_tile));
						}
						brush_sprite.num_tiles = static_cast<Uint16>(brush_sprite.sprite_tiles.size());
						SDLSurfaceHandle new_surface{ SDL_CreateSurface(brush_sprite.GetBoundingBox().Width(), brush_sprite.GetBoundingBox().Height(), SDL_PIXELFORMAT_RGBA32) };
						SDL_SetSurfaceColorKey(new_surface.get(), request.is_chroma_keyed, 0);
						SDL_FillSurfaceRect(new_surface.get(), nullptr, 0);
						brush_sprite.RenderToSurface(new_surface.get());
						SDL_Surface* the_surface = new_surface.get();
						brush_previews.emplace_back(TileBrushPreview{ std::move(new_surface), Renderer::RenderToTexture(the_surface), static_cast<Uint32>(brush_index) });
					}
				}

				for (size_t i = 0; i < m_working_tile_layout->tile_instances.size(); ++i)
				{
					const auto tile_index = m_working_tile_layout->tile_instances[i].tile_index;
					if (tile_index >= m_working_tileset->tiles.size())
					{
						break;
					}

					const rom::Tile& tile = m_working_tileset->tiles[tile_index];
					const rom::TileInstance& tile_instance = m_working_tile_layout->tile_instances[i];
					static SDLSurfaceHandle temp_surface{ SDL_CreateSurface(rom::TileSet::s_tile_width, rom::TileSet::s_tile_height, SDL_PIXELFORMAT_INDEX8) };
					SDLPaletteHandle tile_palette = Renderer::CreateSDLPalette(tile_instance.palette_line == 0 && request.palette_line.has_value() ? *m_working_palette_set.palette_lines.at(*request.palette_line) : *m_working_palette_set.palette_lines.at(tile_instance.palette_line));
					SDL_SetSurfacePalette(temp_surface.get(), tile_palette.get());
					SDL_SetSurfaceColorKey(temp_surface.get(), request.is_chroma_keyed, 0);
					SDL_FillSurfaceRect(temp_surface.get(), nullptr, 0);

					const size_t x_size = rom::TileSet::s_tile_width;
					const size_t y_size = rom::TileSet::s_tile_height;
					if (tile.surface != nullptr)
					{
						SDL_BlitSurface(temp_surface.get(), nullptr, tile.surface.get(), nullptr);
					}
					else
					{
						size_t target_pixel_index = 0;
						for (size_t i = 0; i < tile.pixel_data.size() && i < temp_surface->pitch * temp_surface->h && (i / x_size) < y_size; ++i, target_pixel_index += 1)
						{
							if ((i % x_size) == 0)
							{
								target_pixel_index = temp_surface->pitch * (i / x_size);
							}

							reinterpret_cast<Uint8*>(temp_surface->pixels)[target_pixel_index] = tile.pixel_data[i];
						}
					}

					if (m_working_tile_layout->tile_instances[i].is_flipped_horizontally)
					{
						SDL_FlipSurface(temp_surface.get(), SDL_FLIP_HORIZONTAL);
					}

					if (m_working_tile_layout->tile_instances[i].is_flipped_vertically)
					{
						SDL_FlipSurface(temp_surface.get(), SDL_FLIP_VERTICAL);
					}

					const auto adjusted_layout_width = (request.tile_layout_width * request.tile_brush_width);
					const int x_off = static_cast<int>(i % adjusted_layout_width) * rom::TileSet::s_tile_width;
					const int y_off = static_cast<int>(((i - (i % adjusted_layout_width)) / adjusted_layout_width)) * rom::TileSet::s_tile_height;

					const SDL_Rect target_rect{ x_off, y_off, x_size, y_size };
					SDL_BlitSurface(temp_surface.get(), nullptr, layout_preview_bg_surface.get(), &target_rect);
				}

				if (m_export_result && export_combined == false)
				{
					static char path_buffer[4096];
					sprintf_s(path_buffer, "spinball_%s_%s.png", request.layout_type_name.c_str(), request.layout_layout_name.c_str());
					std::filesystem::path export_path = m_owning_ui.GetSpriteExportPath().append(path_buffer);
					assert(IMG_SavePNG(layout_preview_bg_surface.get(), export_path.generic_u8string().c_str()));
				}

				m_tile_layout_render_requests.erase(std::begin(m_tile_layout_render_requests));
			}

			if (render_flippers)
			{
				for (size_t i = 0; i < m_level->m_flipper_instances.size(); ++i)
				{
					const rom::FlipperInstance& flipper = m_level->m_flipper_instances[i];

					SDLSurfaceHandle temp_surface{ SDL_DuplicateSurface(m_flipper_preview.sprite.get()) };
					int x_off = -24;
					if (flipper.is_x_flipped)
					{
						SDL_FlipSurface(temp_surface.get(), SDL_FLIP_HORIZONTAL);
						x_off = -20;
					}

					SDL_Rect target_rect{ flipper.x_pos + x_off, flipper.y_pos - rom::FlipperInstance::height, rom::FlipperInstance::width, rom::FlipperInstance::height };
					SDL_SetSurfaceColorKey(temp_surface.get(), true, SDL_MapRGBA(SDL_GetPixelFormatDetails(temp_surface->format), nullptr, 0, 0, 0, 0));
					SDL_BlitSurface(temp_surface.get(), nullptr, layout_preview_fg_surface.get(), &target_rect);
				}
			}

			if (render_rings)
			{
				for (size_t i = 0; i < m_level->m_ring_instances.size(); ++i)
				{
					const rom::RingInstance& ring = m_level->m_ring_instances[i];

					SDLSurfaceHandle temp_surface{ SDL_DuplicateSurface(m_ring_preview.sprite.get()) };
					SDL_Rect target_rect{ ring.x_pos + ring.draw_pos_offset.x, ring.y_pos + ring.draw_pos_offset.y, static_cast<int>(ring.dimensions.x), static_cast<int>(ring.dimensions.y) };
					SDL_SetSurfaceColorKey(temp_surface.get(), true, SDL_MapRGBA(SDL_GetPixelFormatDetails(temp_surface->format), nullptr, 0, 0, 0, 0));
					SDL_BlitSurface(temp_surface.get(), nullptr, layout_preview_fg_surface.get(), &target_rect);
				}

				static rom::Colour bbox_colours[]
				{
					{255,255,255,255},
					{255,255,0,0},
					{255,0,255,0},
					{255,16,0,255},
					{255,255,255,0},
					{255,255,0,255},
					{255,0,255,255},
					{255,128,128,128},
					{255,64,192,128},
					{255,192,32,210},
					{255,128,128,255},
					{255,128,128,255},
				};
				static rom::Colour collision_box_colour = { 0, 255, 255, 255 };

				for (size_t i = 0; i < m_level->m_game_obj_instances.size(); ++i)
				{
					const rom::GameObjectDefinition& game_obj = m_level->m_game_obj_instances[i];

					if (game_obj.instance_id == 0)
					{
						std::unique_ptr<UIGameObject> new_obj = std::make_unique<UIGameObject>();
						new_obj->obj_definition = game_obj;
						m_game_object_manager.game_objects.emplace_back(std::move(new_obj));
						continue;
					}

					rom::AnimObjectDefinition anim_obj = rom::AnimObjectDefinition::LoadFromROM(m_owning_ui.GetROM(), m_level->m_game_obj_instances[i].animation_definition);

					auto anim_entry = std::find_if(std::begin(m_anim_sprite_instances), std::end(m_anim_sprite_instances), [&anim_obj](const AnimSpriteEntry& entry)
						{
							return entry.anim_sequence->rom_data.rom_offset == anim_obj.starting_animation;
						});

					if (anim_entry != std::end(m_anim_sprite_instances) && anim_entry->sprite_surface != nullptr)
					{
						const AnimSpriteEntry& animSpriteEntry = *anim_entry;
						const rom::AnimationCommand& command_sprite_came_from = anim_entry->anim_sequence->command_sequence.at(anim_entry->anim_command_used_for_surface);
						auto origin_offset = command_sprite_came_from.ui_frame_sprite->sprite->GetOriginOffsetFromMinBounds();

						SDLSurfaceHandle temp_sprite_surface{ SDL_ScaleSurface(anim_entry->sprite_surface.get(), static_cast<int>(command_sprite_came_from.ui_frame_sprite->dimensions.x), static_cast<int>(command_sprite_came_from.ui_frame_sprite->dimensions.y), SDL_SCALEMODE_NEAREST) };
						SDL_Rect sprite_target_rect{ game_obj.x_pos - origin_offset.x, game_obj.y_pos - origin_offset.y, anim_entry->sprite_surface->w, anim_entry->sprite_surface->h };

						if (game_obj.FlipX())
						{
							SDL_FlipSurface(temp_sprite_surface.get(), SDL_FLIP_HORIZONTAL);
							sprite_target_rect.x = game_obj.x_pos - (anim_entry->sprite_surface->w - origin_offset.x);
						}

						if (game_obj.FlipY())
						{
							SDL_FlipSurface(temp_sprite_surface.get(), SDL_FLIP_VERTICAL);
							sprite_target_rect.y = game_obj.y_pos - (anim_entry->sprite_surface->h - origin_offset.y);
						}

						SDL_SetSurfaceColorKey(temp_sprite_surface.get(), true, 0);
						SDL_BlitSurfaceScaled(temp_sprite_surface.get(), nullptr, layout_preview_bg_surface.get(), &sprite_target_rect, SDL_SCALEMODE_NEAREST);

						std::unique_ptr<UIGameObject> new_obj = std::make_unique<UIGameObject>();
						new_obj->obj_definition = game_obj;
						new_obj->sprite_pos_offset = ImVec2{ static_cast<float>(sprite_target_rect.x) - game_obj.x_pos, static_cast<float>(sprite_target_rect.y) - game_obj.y_pos };
						new_obj->dimensions = ImVec2{ static_cast<float>(sprite_target_rect.w) , static_cast<float>(sprite_target_rect.h) };
						new_obj->ui_sprite = command_sprite_came_from.ui_frame_sprite;
						new_obj->ui_sprite->texture = new_obj->ui_sprite->RenderTextureForPalette(UIPalette{ *m_working_palette_set.palette_lines.at(anim_obj.palette_index % 4).get() });
						new_obj->sprite_table_address = animSpriteEntry.sprite_table;
						m_game_object_manager.game_objects.emplace_back(std::move(new_obj));
					}
					else
					{
						SDLSurfaceHandle temp_surface{ SDL_ScaleSurface(m_game_object_preview.sprite.get(), game_obj.collision_width, game_obj.collision_height, SDL_SCALEMODE_NEAREST) };

						auto cutting_surface = SDLSurfaceHandle{ SDL_CreateSurface(temp_surface->w, temp_surface->h, SDL_PIXELFORMAT_RGBA32) };
						SDL_ClearSurface(temp_surface.get(), bbox_colours[game_obj.type_id % std::size(bbox_colours)].r / 256.0f, bbox_colours[game_obj.type_id % std::size(bbox_colours)].g / 256.0f, bbox_colours[game_obj.type_id % std::size(bbox_colours)].b / 256.0f, 255.0f);
						SDL_ClearSurface(cutting_surface.get(), 255.0f, 0.0f, 0.0f, 255.0f);
						const SDL_Rect cutting_target_rect{ 2,2,temp_surface->w - 4, temp_surface->h - 4 };
						SDL_BlitSurfaceScaled(cutting_surface.get(), nullptr, temp_surface.get(), &cutting_target_rect, SDL_SCALEMODE_NEAREST);

						SDL_Rect target_rect{ game_obj.x_pos - game_obj.collision_width / 2, game_obj.y_pos - game_obj.collision_height, game_obj.collision_width, game_obj.collision_height };
						SDL_SetSurfaceColorKey(temp_surface.get(), true, SDL_MapRGBA(SDL_GetPixelFormatDetails(temp_surface->format), nullptr, 255, 0, 0, 255));
						SDL_BlitSurfaceScaled(temp_surface.get(), nullptr, layout_preview_bg_surface.get(), &target_rect, SDL_SCALEMODE_NEAREST);

						std::unique_ptr<UIGameObject> new_obj = std::make_unique<UIGameObject>();
						new_obj->obj_definition = game_obj;
						new_obj->sprite_pos_offset = ImVec2{ static_cast<float>(target_rect.x) - game_obj.x_pos, static_cast<float>(target_rect.y) - game_obj.y_pos };
						new_obj->dimensions = ImVec2{ static_cast<float>(target_rect.w) , static_cast<float>(target_rect.h) };
						//new_obj->ui_sprite = command_sprite_came_from.ui_frame_sprite;
						//new_obj->sprite_table_address = animSpriteEntry.sprite_table;
						m_game_object_manager.game_objects.emplace_back(std::move(new_obj));
					}
				}

				if (m_level->m_data_offsets.collision_tile_obj_ids.offset != 0)
				{
					rom::GameObjectCullingTable game_obj_table = rom::GameObjectCullingTable::LoadFromROM(m_owning_ui.GetROM(), m_level->m_data_offsets.collision_tile_obj_ids.offset);
					for (Uint32 sector_index = 0; sector_index < game_obj_table.cells.size() - 1; ++sector_index)
					{
						const rom::GameObjectCullingCell& cell = game_obj_table.cells[sector_index];
						for (const std::unique_ptr<UIGameObject>& game_object : m_game_object_manager.game_objects)
						{
							for (const Uint16 obj_id : cell.obj_instance_ids)
							{
								if (obj_id == game_object->obj_definition.instance_id)
								{
									game_object->had_collision_sectors_on_rom = true;
								}
							}
						}
					}
				}

				rom::AnimatedObjectCullingTable anim_obj_table = rom::AnimatedObjectCullingTable::LoadFromROM(m_owning_ui.GetROM(), m_owning_ui.GetROM().ReadUint32(m_level->m_data_offsets.camera_activation_sector_anim_obj_ids));
				for (Uint32 sector_index = 0; sector_index < anim_obj_table.cells.size() - 1; ++sector_index)
				{
					const rom::AnimatedObjectCullingCell& cell = anim_obj_table.cells[sector_index];
					for (const std::unique_ptr<UIGameObject>& game_object : m_game_object_manager.game_objects)
					{
						for (const Uint16 obj_id : cell.obj_instance_ids)
						{
							if (obj_id == game_object->obj_definition.instance_id)
							{
								game_object->had_culling_sectors_on_rom = true;
							}
						}
					}
				}
			}

			if (m_export_result && export_combined)
			{
				static char path_buffer[4096];

				sprintf_s(path_buffer, "spinball_%s.png", combined_type_name.c_str());
				std::filesystem::path export_path = m_owning_ui.GetSpriteExportPath().append(path_buffer);
				if (export_combined)
				{
					SDLSurfaceHandle combined{ SDL_DuplicateSurface(layout_preview_bg_surface.get()) };
					SDL_BlitSurface(layout_preview_fg_surface.get(), nullptr, combined.get(), nullptr);
					assert(IMG_SavePNG(combined.get(), export_path.generic_u8string().c_str()));
				}
				else
				{
					assert(IMG_SavePNG(layout_preview_bg_surface.get(), export_path.generic_u8string().c_str()));
				}
			}

			if (will_be_rendering_preview)
			{
				m_tile_layout_preview_bg = Renderer::RenderToTexture(layout_preview_bg_surface.get());
				m_tile_layout_preview_fg = Renderer::RenderToTexture(layout_preview_fg_surface.get());
			}

			bool has_just_selected_item = false;

			//////////// BEGIN IMGUI ///////////////////////////////////

			ImGui::BeginGroup();
			{
				if (ImGui::BeginChild("InfoSizebar", ImVec2{ 340, -1 }, 0, ImGuiWindowFlags_AlwaysVerticalScrollbar))
				{
					ImGui::SliderFloat("Zoom", &m_zoom, 0, 8.0f, "%.1f");
					ImGui::SliderInt("Grid Snap", &m_grid_snap, 1, 128);

					constexpr size_t preview_brushes_per_row = 8;
					if (current_preview_data && ImGui::TreeNode("ROM Info"))
					{
						const auto address_end = current_preview_data->tile_layout_address_end.value_or(current_preview_data->tile_layout_address + (current_preview_data->tile_layout_width * 2) * current_preview_data->tile_layout_height);
						ImGui::Text("Tileset Compressed Data: 0x%08X", current_preview_data->tileset_address);
						ImGui::Text("Tileset Brushes: 0x%08X => 0x%08X", current_preview_data->tile_brushes_address, current_preview_data->tile_brushes_address_end);
						ImGui::Text("Tile Layout: 0x%08X => 0x%08X", current_preview_data->tile_layout_address, address_end);
						ImGui::Text("Layout size: %d", address_end - current_preview_data->tile_layout_address);
						ImGui::Text("Width: %d", current_preview_data->tile_layout_width);
						ImGui::Text("Height: %d", current_preview_data->tile_layout_height);
						ImGui::Text("Num tiles: %d", current_preview_data->tile_layout_width * current_preview_data->tile_layout_height);
						ImGui::TreePop();
					}

					if (ImGui::BeginTabBar("sidebar_primary_tabs"))
					{
						if (ImGui::BeginTabItem("Level Info"))
						{
							if (m_level != nullptr)
							{
								DrawLevelInfo();
							}
							ImGui::EndTabItem();
						}

						if (ImGui::BeginTabItem("Objects"))
						{
							if (ImGui::BeginTabBar("objects_children"))
							{
								if (ImGui::BeginTabItem("Game Objects"))
								{
									DrawObjectTable();
									ImGui::EndTabItem();
								}

								if (ImGui::BeginTabItem("Flippers"))
								{
									DrawFlippersTable();
									ImGui::EndTabItem();
								}

								if (ImGui::BeginTabItem("Rings"))
								{
									DrawRingsTable();
									ImGui::EndTabItem();
								}
								ImGui::EndTabBar();
							}
							ImGui::EndTabItem();
						}

						bool request_open_brush_popup = false;

						if (m_tile_picker_list.empty() == false && ImGui::BeginTabItem("Tilesets"))
						{
							if (ImGui::BeginTabBar("tile_layers"))
							{
								static const char* layer_names[] =
								{
									"Background",
									"Foreground"
								};

								int tab_index = 0;
								for (Uint16 layer_index = 0; layer_index < m_tile_picker_list.size(); ++layer_index)
								{
									TilePicker& tile_picker = m_tile_picker_list[layer_index];
									tile_picker.SetTileLayer(m_level != nullptr ? &m_level->m_tile_layers[layer_index] : nullptr);
									if (ImGui::BeginTabItem(layer_names[tab_index++]))
									{
										if (ImGui::Button("Pick from layout"))
										{
											m_selected_tile.is_picking_from_layout = true;
											m_selected_tile.tile_layer = m_level ? &m_level->m_tile_layers[layer_index] : nullptr;
											m_selected_tile.tile_picker = &tile_picker;
										}

										const bool had_selection = tile_picker.currently_selected_tile != nullptr;
										tile_picker.Draw();

										if (had_selection == false && tile_picker.currently_selected_tile != nullptr)
										{
											m_selected_tile.tile_selection = tile_picker.GetSelectedTile();
											m_selected_tile.tile_layer = m_level ? &m_level->m_tile_layers[layer_index] : nullptr;
											m_selected_tile.tile_picker = &tile_picker;
											has_just_selected_item = true;
										}

										ImGui::EndTabItem();
									}
								}
								ImGui::EndTabBar();
							}
							ImGui::EndTabItem();
						}

						if (m_tileset_preview_list.empty() == false && ImGui::BeginTabItem("Brushes"))
						{
							if (ImGui::BeginTabBar("tile_layers"))
							{
								static const char* layer_names[] =
								{
									"Background",
									"Foreground"
								};
								int tab_index = 0;
								constexpr size_t num_previews_per_page = 8 * 16;
								for (Uint16 layer_index = 0; layer_index < m_tileset_preview_list.size(); ++layer_index)
								{
									TilesetPreview& tileset_preview = m_tileset_preview_list[layer_index];
									if (ImGui::BeginTabItem(layer_names[tab_index++]))
									{
										ImGui::PushID(&tileset_preview);
										ImGui::BeginDisabled((tileset_preview.current_page - 1) < 0);
										if (ImGui::Button("Previous Page"))
										{
											tileset_preview.current_page = std::max(0, tileset_preview.current_page - 1);
										}
										ImGui::EndDisabled();
										ImGui::SameLine();
										ImGui::Text("%d / %d", tileset_preview.current_page + 1, num_previews_per_page != 0 ? static_cast<int>(tileset_preview.brushes.size()) / num_previews_per_page + 1 : 1);
										ImGui::SameLine();
										ImGui::BeginDisabled((tileset_preview.current_page + 1) * num_previews_per_page >= tileset_preview.brushes.size());
										if (ImGui::Button("Next Page"))
										{
											tileset_preview.current_page = std::min<int>((static_cast<int>(tileset_preview.brushes.size()) / num_previews_per_page), tileset_preview.current_page + 1);
										}
										ImGui::EndDisabled();
										if (ImGui::Button("Pick from layout"))
										{
											m_selected_brush.is_picking_from_layout = true;
											m_selected_brush.tile_layer = m_level ? &m_level->m_tile_layers[layer_index] : nullptr;
											m_selected_brush.tileset = &tileset_preview;
										}

										for (size_t page_index = tileset_preview.current_page * num_previews_per_page; page_index < std::min<size_t>((tileset_preview.current_page + 1) * num_previews_per_page, tileset_preview.brushes.size()); ++page_index)
										{
											TileBrushPreview& preview_brush = tileset_preview.brushes[page_index];
											if (preview_brush.texture != nullptr)
											{
												if (page_index % preview_brushes_per_row != 0)
												{
													ImGui::SameLine();
												}

												ImGui::Image((ImTextureID)preview_brush.texture.get(), ImVec2(static_cast<float>(preview_brush.texture->w) * m_zoom, static_cast<float>(preview_brush.texture->h) * m_zoom));
												if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
												{
													m_selected_brush.tileset = &tileset_preview;
													m_selected_brush.tile_brush = &preview_brush;
													m_selected_brush.tile_layer = m_level ? &m_level->m_tile_layers[layer_index] : nullptr;
													has_just_selected_item = true;
												}

												if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
												{
													m_working_brush = preview_brush.brush_index;
													m_working_layer_index = layer_index;
													request_open_brush_popup = true;
												}

												if (m_working_brush.has_value() == false)
												{
													if (ImGui::BeginItemTooltip())
													{
														ImGui::Text("Tile Index: 0x%02X", page_index);
														ImGui::EndTooltip();
													}
												}
											}
										}
										ImGui::PopID();
										ImGui::EndTabItem();
									}
								}
								ImGui::EndTabBar();
							}
							ImGui::EndTabItem();
						}

						if (request_open_brush_popup == true)
						{
							ImGui::OpenPopup("brush_edit_popup");
						}

						if (m_working_brush.has_value() && m_working_layer_index.has_value())
						{
							if (ImGui::IsPopupOpen("brush_edit_popup") == false)
							{
								m_working_brush.reset();
								m_working_layer_index.reset();
							}

							if (ImGui::BeginPopup("brush_edit_popup"))
							{
								if (ImGui::Selectable("Edit brush"))
								{
									m_brush_editor.emplace(m_owning_ui, m_level->m_tile_layers[m_working_layer_index.value()], m_working_brush.value());
									m_brush_editor->m_visible = true;
								}
								ImGui::EndPopup();
							}
						}

						if (ImGui::BeginTabItem("Palettes"))
						{
							if (m_level != nullptr)
							{
								for (const rom::TileLayer& tile_layer : m_level->m_tile_layers)
								{
									for (const std::shared_ptr<rom::Palette>& palette : tile_layer.palette_set.palette_lines)
									{
										if (palette != nullptr)
										{
											DrawPaletteSwatchPreview(*palette);
										}
									}
								}
							}
							else
							{
								for (const std::shared_ptr<rom::Palette>& palette : m_working_palette_set.palette_lines)
								{
									if (palette != nullptr)
									{
										DrawPaletteSwatchPreview(*palette);
									}
								}
							}
							ImGui::EndTabItem();
						}

						ImGui::EndTabBar();
					}
				}
				ImGui::EndChild();
			}
			ImGui::EndGroup();

			ImGui::SameLine();

			ImGui::BeginGroup();
			{
				if (ImGui::BeginChild("Preview info Area", ImVec2{ 0,0 }, 0, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar))
				{
					const ImVec2 panel_screen_origin = ImGui::GetCursorScreenPos();
					ImGuiWindow* tile_area = ImGui::GetCurrentContext()->CurrentWindow;
					const bool is_hovering_tile_area = (ImGui::GetCurrentContext()->HoveredWindow == tile_area);
					if (m_tile_layout_preview_bg != nullptr || m_tile_layout_preview_fg != nullptr)
					{
						LayerSettings current_layer_settings = m_layer_settings;

						const ImVec2 origin{ ImGui::GetCursorPos() };
						ImVec2 screen_origin{ ImGui::GetCursorScreenPos() };
						const ImVec2 mouse_pos{ ImGui::GetMousePos() };
						const ImVec2 relative_mouse_pos{ (mouse_pos - panel_screen_origin) + origin };

						const ImVec2 tile_dimensions{ rom::TileSet::s_tile_width, rom::TileSet::s_tile_height };
						const ImVec2 tile_grid_pos{ static_cast<float>(static_cast<int>(relative_mouse_pos.x / (tile_dimensions.x * m_zoom))), static_cast<float>(static_cast<int>(relative_mouse_pos.y / (tile_dimensions.y * m_zoom))) };
						const bool is_tile_grid_pos_within_bounds = m_level == nullptr || m_level->m_tile_layers.empty() || tile_grid_pos.x >= 0 && tile_grid_pos.x < (m_level->m_tile_layers[0].tile_layout->layout_width * 4) && tile_grid_pos.y >= 0 && (tile_grid_pos.y < m_level->m_tile_layers[0].tile_layout->layout_height * 4);
						const ImVec2 tile_snapped_pos{ (tile_grid_pos * tile_dimensions) * m_zoom };
						const ImVec2 tile_final_snapped_pos{ tile_snapped_pos + screen_origin + (panel_screen_origin - screen_origin) };

						const ImVec2 tile_brush_dimensions = ImVec2{ rom::TileBrush<4, 4>::s_brush_width, rom::TileBrush<4,4>::s_brush_height } *tile_dimensions;
						const ImVec2 tile_brush_grid_pos{ static_cast<float>(static_cast<int>(relative_mouse_pos.x / (tile_brush_dimensions.x * m_zoom))), static_cast<float>(static_cast<int>(relative_mouse_pos.y / (tile_brush_dimensions.y * m_zoom))) };
						const bool is_tile_brush_grid_pos_within_bounds = m_level == nullptr || m_level->m_tile_layers.empty() || tile_brush_grid_pos.x >= 0 && tile_brush_grid_pos.x < m_level->m_tile_layers[0].tile_layout->layout_width && tile_brush_grid_pos.y >= 0 && tile_brush_grid_pos.y < m_level->m_tile_layers[0].tile_layout->layout_height;
						const ImVec2 tile_brush_snapped_pos{ (tile_brush_grid_pos * tile_brush_dimensions) * m_zoom };
						const ImVec2 tile_brush_final_snapped_pos{ tile_brush_snapped_pos + screen_origin + (panel_screen_origin - screen_origin) };

						const float max_layout_width = m_level == nullptr ? std::max<float>(static_cast<float>(m_tile_layout_preview_bg->w), static_cast<float>(m_tile_layout_preview_fg->w)) : std::max(static_cast<float>(m_level->m_tile_layers[0].tile_layout->layout_width) * tile_brush_dimensions.x, static_cast<float>(m_level->m_tile_layers[1].tile_layout->layout_width) * tile_brush_dimensions.y);
						const float max_layout_height = m_level == nullptr ? std::max<float>(static_cast<float>(m_tile_layout_preview_bg->h), static_cast<float>(m_tile_layout_preview_fg->h)) : std::max(static_cast<float>(m_level->m_tile_layers[0].tile_layout->layout_height) * tile_brush_dimensions.x, static_cast<float>(m_level->m_tile_layers[1].tile_layout->layout_height) * tile_brush_dimensions.y);

						const ImVec2 level_dimensions{ max_layout_width, max_layout_height };
						const ImVec2 zoomed_level_dimensions{ level_dimensions * m_zoom };

						ImGui::Image((ImTextureID)m_tile_layout_preview_bg.get(), zoomed_level_dimensions, ImVec2{ 0, 0 }, level_dimensions / ImVec2{ static_cast<float>(m_tile_layout_preview_bg->w),  static_cast<float>(m_tile_layout_preview_bg->h) });
						ImGui::SetCursorPos(origin);
						ImGui::Image((ImTextureID)m_tile_layout_preview_fg.get(), zoomed_level_dimensions, ImVec2{ 0, 0 }, level_dimensions / ImVec2{ static_cast<float>(m_tile_layout_preview_fg->w),  static_cast<float>(m_tile_layout_preview_fg->h) });

						// Visualise collision vectors
						constexpr int collision_sector_width = 128;
						constexpr int num_collision_sectors_x = 16;
						constexpr int size_of_preview_collision_boxes = 4;
						constexpr int half_size_of_preview_collision_boxes = size_of_preview_collision_boxes / 2;

						int selected_spline_culling_cell_index = -1;

						if (current_layer_settings.spline_culling == true)
						{
							selected_spline_culling_cell_index = (static_cast<int>((relative_mouse_pos.x / m_zoom) / rom::SplineCullingTable::cell_dimensions.x) % (rom::SplineCullingTable::grid_dimensions.x))
								+ (static_cast<int>((relative_mouse_pos.y / m_zoom) / rom::SplineCullingTable::cell_dimensions.y) * (rom::SplineCullingTable::grid_dimensions.x));

							if (selected_spline_culling_cell_index != -1)
							{
								const int collision_tile_origin_x = (static_cast<int>(selected_spline_culling_cell_index) % rom::SplineCullingTable::grid_dimensions.x) * rom::SplineCullingTable::cell_dimensions.x;
								const int collision_tile_origin_y = (static_cast<int>(selected_spline_culling_cell_index) / rom::SplineCullingTable::grid_dimensions.x) * rom::SplineCullingTable::cell_dimensions.y;

								ImGui::GetWindowDrawList()->AddRect(
									ImVec2{ static_cast<float>(screen_origin.x + (collision_tile_origin_x * m_zoom)), static_cast<float>(screen_origin.y + (collision_tile_origin_y * m_zoom)) },
									ImVec2{ static_cast<float>(screen_origin.x + ((collision_tile_origin_x + rom::SplineCullingTable::cell_dimensions.x) * m_zoom)), static_cast<float>(screen_origin.y + ((collision_tile_origin_y + rom::SplineCullingTable::cell_dimensions.y) * m_zoom)) },
									ImGui::GetColorU32(ImVec4{ 64,64,64,255 }), 0, ImDrawFlags_None, 1.0f);

							}

							if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
							{
								m_layer_settings.spline_culling = false;
							}
						}

						if (selected_spline_culling_cell_index != -1)
						{
							current_layer_settings.collision = true;
							current_layer_settings.rings = false;
							current_layer_settings.flippers = false;
							current_layer_settings.visible_objects = false;
							current_layer_settings.hover_game_objects = false;
							current_layer_settings.hover_splines = false;
							current_layer_settings.hover_radials = false;
							current_layer_settings.hover_brushes = false;
						}

						if (m_selected_brush.HasSelection() || m_selected_brush.IsPickingFromLayout() || m_selected_tile.HasSelection() || m_selected_tile.IsPickingFromLayout())
						{
							current_layer_settings.collision = false;
							current_layer_settings.rings = false;
							current_layer_settings.flippers = false;
							current_layer_settings.visible_objects = false;
							current_layer_settings.hover_game_objects = false;
							current_layer_settings.hover_splines = false;
							current_layer_settings.hover_radials = false;
							current_layer_settings.hover_brushes = m_selected_brush.IsPickingFromLayout();
						}

						if (m_working_game_obj)
						{
							current_layer_settings.collision = false;
							current_layer_settings.rings = false;
							current_layer_settings.flippers = false;
							current_layer_settings.visible_objects = false;
							current_layer_settings.hover_game_objects = false;
							current_layer_settings.hover_splines = false;
							current_layer_settings.hover_radials = false;
							current_layer_settings.hover_brushes = false;
						}

						if (current_layer_settings.collision == true)
						{
							bool has_drawn_working_spline = false;
							for (rom::CollisionSpline& next_spline : m_spline_manager.splines)
							{
								if (current_layer_settings.spline_culling && selected_spline_culling_cell_index >= 0 && selected_spline_culling_cell_index < rom::SplineCullingTable::cells_count)
								{
									const bool spline_is_visible = std::any_of(std::begin(m_working_culling_table.cells[selected_spline_culling_cell_index].splines), std::end(m_working_culling_table.cells[selected_spline_culling_cell_index].splines),
										[&next_spline](const rom::CollisionSpline& spline)
										{
											return spline == next_spline;
										});
									if (spline_is_visible == false)
									{
										continue;
									}
								}

								const bool is_working_spline = (m_working_spline.has_value() && m_working_spline->destination == &next_spline && (m_working_spline->dest_spline_point != nullptr || ImGui::IsPopupOpen("spline_popup")));
								rom::CollisionSpline& spline = is_working_spline ? m_working_spline->spline : next_spline;
								has_drawn_working_spline |= is_working_spline;
								DrawCollisionSpline(spline, origin, screen_origin, current_layer_settings, is_working_spline);
							}

							if (has_drawn_working_spline == false && m_working_spline.has_value())
							{
								rom::CollisionSpline spline = m_working_spline->spline;
								DrawCollisionSpline(spline, origin, screen_origin, current_layer_settings, true);
							}
						}

						if (m_level && has_just_selected_item == false)
						{
							const int brush_grid_ref = static_cast<int>((tile_brush_grid_pos.y * m_level->m_tile_layers[0].tile_layout->layout_width) + tile_brush_grid_pos.x);
							const int tile_grid_ref = static_cast<int>((tile_grid_pos.y * m_level->m_tile_layers[0].tile_layout->layout_width * 4) + tile_grid_pos.x);
							if (/*current_layer_settings.hover_tiles &&*/m_selected_brush.tile_layer && brush_grid_ref >= 0 && brush_grid_ref < m_selected_brush.tile_layer->tile_layout->tile_brush_instances.size())
							{
								const rom::TileBrushInstance& brush_instance = m_selected_brush.tile_layer->tile_layout->tile_brush_instances.at(brush_grid_ref);
								const int hovered_index = brush_instance.tile_brush_index;

								if (m_selected_brush.tileset != nullptr && hovered_index >= 0 && hovered_index < m_selected_brush.tileset->brushes.size() && ImGui::BeginTooltip())
								{
									const ImVec2 uv_min{ brush_instance.is_flipped_horizontally ? 1.0f : 0.0f, brush_instance.is_flipped_vertically ? 1.0f : 0.0f };
									const ImVec2 uv_max{ brush_instance.is_flipped_horizontally ? 0.0f : 1.0f, brush_instance.is_flipped_vertically ? 0.0f : 1.0f };
									ImGui::Image((ImTextureID)m_selected_brush.tileset->brushes.at(hovered_index).texture.get(), ImVec2{ 64,64 }, uv_min, uv_max);
									ImGui::EndTooltip();
								}
							}

							if (m_selected_tile.IsActive() && is_tile_grid_pos_within_bounds)
							{
								const ImVec2 rect_min{ tile_final_snapped_pos.x - 1, tile_final_snapped_pos.y - 1 };
								const ImVec2 rect_max{ tile_final_snapped_pos.x + (tile_dimensions.x * m_zoom) + 1, tile_final_snapped_pos.y + (tile_dimensions.y * m_zoom) + 1 };

								if (m_selected_tile.HasSelection())
								{
									ImGui::SetCursorScreenPos(tile_final_snapped_pos);
									m_selected_tile.tile_picker->DrawPickedTile(m_selected_tile.flip_x, m_selected_tile.flip_y, m_zoom);

									if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
									{
										m_selected_tile.tile_picker->currently_selected_tile = nullptr;
										m_selected_tile.Clear();
										has_just_selected_item = true;
									}
								}

								ImVec2 min_pos;
								ImVec2 max_pos;

								if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && m_selected_tile.dragging_start_ref.has_value())
								{
									const ImVec2 start_grid_pos = m_selected_tile.dragging_start_ref.value();
									const ImVec2 end_grid_pos = tile_grid_pos;

									const float start_x = std::min(start_grid_pos.x, end_grid_pos.x);
									const float end_x = std::max(start_grid_pos.x, end_grid_pos.x);
									const float start_y = std::min(start_grid_pos.y, end_grid_pos.y);
									const float end_y = std::max(start_grid_pos.y, end_grid_pos.y);

									{
										const ImVec2 snapped_pos{ (ImVec2{start_x,start_y} *tile_dimensions) * m_zoom };
										const ImVec2 final_snapped_pos{ snapped_pos + screen_origin + (panel_screen_origin - screen_origin) };
										min_pos = final_snapped_pos;
									}
									{
										const ImVec2 snapped_pos{ (ImVec2{end_x + 1,end_y + 1} *tile_dimensions) * m_zoom };
										const ImVec2 final_snapped_pos{ snapped_pos + screen_origin + (panel_screen_origin - screen_origin) };
										max_pos = final_snapped_pos;
									}

									for (float x = start_x; x <= end_x; ++x)
									{
										for (float y = start_y; y <= end_y; ++y)
										{
											const ImVec2 snapped_pos{ (ImVec2{x,y} *tile_dimensions) * m_zoom };
											const ImVec2 final_snapped_pos{ snapped_pos + screen_origin + (panel_screen_origin - screen_origin) };
											ImGui::SetCursorScreenPos(final_snapped_pos);
											m_selected_tile.tile_picker->DrawPickedTile(m_selected_tile.flip_x, m_selected_tile.flip_y, m_zoom);
										}
									}

									ImGui::GetWindowDrawList()->AddRect(min_pos, max_pos, ImGui::GetColorU32(ImVec4{ 1.0f,0.0f,1.0f,1.0f }), 0, 0, 1.0f);
								}
								else
								{
									ImGui::GetForegroundDrawList()->AddRect(rect_min, rect_max, ImGui::GetColorU32(ImVec4{ 1.0f, 0.0f, 1.0f, 1.0f }), 0, 0, 1.0f);
								}

								if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && m_selected_tile.tile_layer != nullptr)
								{
									if (m_selected_tile.IsPickingFromLayout())
									{
										if (m_selected_tile.tile_layer->tile_layout->tile_instances.empty() == false && tile_grid_ref < m_selected_tile.tile_layer->tile_layout->tile_instances.size())
										{
											const rom::TileInstance& tile_instance = m_selected_tile.tile_layer->tile_layout->tile_instances.at(tile_grid_ref);
											const int selected_index = tile_instance.tile_index;
											m_selected_tile.tile_selection = &m_selected_tile.tile_layer->tileset->tiles[selected_index];
											m_selected_tile.tile_picker->currently_selected_tile = m_selected_tile.tile_picker->tiles[selected_index].get();
											m_selected_tile.tile_picker->SetPaletteLine(tile_instance.palette_line);
											m_selected_tile.flip_x = tile_instance.is_flipped_horizontally;
											m_selected_tile.flip_y = tile_instance.is_flipped_vertically;

											m_selected_tile.is_picking_from_layout = false;
											m_selected_tile.was_picked_from_layout = true;
										}
										else
										{
											m_selected_tile.Clear();
										}
										has_just_selected_item = true;
									}
									else if (m_selected_tile.HasSelection())
									{
										if (tile_grid_ref < m_selected_tile.tile_layer->tile_layout->tile_instances.size())
										{
											m_selected_tile.dragging_start_ref = tile_grid_pos;
										}
									}
								}

								if (m_selected_tile.IsPickingFromLayout() == false && m_selected_tile.tile_selection == nullptr)
								{
									m_selected_tile.Clear();
								}

								if (m_selected_tile.HasSelection())
								{
									if (ImGui::IsKeyPressed(ImGuiKey_R))
									{
										m_selected_tile.flip_x = !m_selected_tile.flip_x;
									}

									if (ImGui::IsKeyPressed(ImGuiKey_F))
									{
										m_selected_tile.flip_y = !m_selected_tile.flip_y;
									}
								}

								if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && m_selected_tile.dragging_start_ref.has_value())
								{
									const ImVec2 start_grid_pos = m_selected_tile.dragging_start_ref.value();
									const ImVec2 end_grid_pos = tile_grid_pos;
									for (float x = std::min(start_grid_pos.x, end_grid_pos.x); x <= std::max(start_grid_pos.x, end_grid_pos.x); ++x)
									{
										for (float y = std::min(start_grid_pos.y, end_grid_pos.y); y <= std::max(start_grid_pos.y, end_grid_pos.y); ++y)
										{
											const size_t tile_index_to_edit = static_cast<size_t>(x) + (static_cast<size_t>(y) * m_selected_tile.tile_layer->tile_layout->layout_width * 4);
											rom::TileInstance& target_tile = m_selected_tile.tile_layer->tile_layout->tile_instances[tile_index_to_edit];
											target_tile.tile_index = static_cast<int>(m_selected_tile.tile_picker->GetSelectedTileIndex());
											target_tile.palette_line = m_selected_tile.tile_picker->current_palette_line;
											target_tile.is_flipped_horizontally = m_selected_tile.flip_x;
											target_tile.is_flipped_vertically = m_selected_tile.flip_y;
										}
									}
									m_render_from_edit = true;
									m_selected_tile.dragging_start_ref.reset();
								}
							}

							if (m_selected_brush.IsActive() && is_tile_brush_grid_pos_within_bounds)
							{
								const ImVec2 rect_min{ tile_brush_final_snapped_pos.x - 1, tile_brush_final_snapped_pos.y - 1 };
								const ImVec2 rect_max{ tile_brush_final_snapped_pos.x + (tile_brush_dimensions.x * m_zoom) + 1, tile_brush_final_snapped_pos.y + (tile_brush_dimensions.y * m_zoom) + 1 };

								ImGui::SetCursorScreenPos(tile_brush_final_snapped_pos);
								ImVec2 uv0 = { 0,0 };
								ImVec2 uv1 = { 1,1 };

								if (m_selected_brush.HasSelection())
								{
									if (m_selected_brush.flip_x)
									{
										uv0.x = 1;
										uv1.x = 0;
									}

									if (m_selected_brush.flip_y)
									{
										uv0.y = 1;
										uv1.y = 0;
									}
								}
								ImVec2 min_pos;
								ImVec2 max_pos;

								if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && m_selected_brush.dragging_start_ref.has_value())
								{
									const ImVec2 start_grid_pos = m_selected_brush.dragging_start_ref.value();
									const ImVec2 end_grid_pos = tile_brush_grid_pos;

									const float start_x = std::min(start_grid_pos.x, end_grid_pos.x);
									const float end_x = std::max(start_grid_pos.x, end_grid_pos.x);
									const float start_y = std::min(start_grid_pos.y, end_grid_pos.y);
									const float end_y = std::max(start_grid_pos.y, end_grid_pos.y);

									{
										const ImVec2 snapped_pos{ (ImVec2{start_x,start_y} *tile_brush_dimensions) * m_zoom };
										const ImVec2 final_snapped_pos{ snapped_pos + screen_origin + (panel_screen_origin - screen_origin) };
										min_pos = final_snapped_pos;
									}
									{
										const ImVec2 snapped_pos{ (ImVec2{end_x + 1,end_y + 1} *tile_brush_dimensions) * m_zoom };
										const ImVec2 final_snapped_pos{ snapped_pos + screen_origin + (panel_screen_origin - screen_origin) };
										max_pos = final_snapped_pos;
									}

									if (m_selected_brush.HasSelection() == true)
									{
										for (float x = start_x; x <= end_x; ++x)
										{
											for (float y = start_y; y <= end_y; ++y)
											{
												const ImVec2 snapped_pos{ (ImVec2{x,y} *tile_brush_dimensions) * m_zoom };
												const ImVec2 final_snapped_pos{ snapped_pos + screen_origin + (panel_screen_origin - screen_origin) };
												ImGui::SetCursorScreenPos(final_snapped_pos);
												ImGui::Image((ImTextureID)m_selected_brush.tile_brush->texture.get(), ImVec2{ static_cast<float>(m_selected_brush.tile_brush->texture->w), static_cast<float>(m_selected_brush.tile_brush->texture->h) } *m_zoom, uv0, uv1);
											}
										}
									}

									ImGui::GetWindowDrawList()->AddRect(min_pos, max_pos, ImGui::GetColorU32(ImVec4{ 1.0f,0.0f,1.0f,1.0f }), 0, 0, 1.0f);
								}
								else if( m_selected_brush.HasSelection())
								{
									ImGui::Image((ImTextureID)m_selected_brush.tile_brush->texture.get(), ImVec2{ static_cast<float>(m_selected_brush.tile_brush->texture->w), static_cast<float>(m_selected_brush.tile_brush->texture->h) } *m_zoom, uv0, uv1);
								}

								if (m_selected_brush.dragging_start_ref.has_value() == false)
								{
									ImGui::GetForegroundDrawList()->AddRect(rect_min, rect_max, ImGui::GetColorU32(ImVec4{ 1.0f, 0.0f, 1.0f, 1.0f }), 0, 0, 1.0f);
								}

								if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && m_selected_brush.tile_layer != nullptr)
								{
									if (brush_grid_ref < m_selected_brush.tile_layer->tile_layout->tile_brush_instances.size())
									{
										m_selected_brush.dragging_start_ref = tile_brush_grid_pos;
									}
								}

								if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && m_selected_brush.dragging_start_ref.has_value())
								{
									const ImVec2 start_grid_pos = m_selected_brush.dragging_start_ref.value();
									const ImVec2 end_grid_pos = tile_brush_grid_pos;

									if (m_selected_brush.IsPickingFromLayout())
									{
										if (m_selected_brush.tile_layer->tile_layout->tile_brush_instances.empty() == false && brush_grid_ref < m_selected_brush.tile_layer->tile_layout->tile_brush_instances.size())
										{
											const rom::TileBrushInstance& brush_instance = m_selected_brush.tile_layer->tile_layout->tile_brush_instances.at(brush_grid_ref);
											const int selected_index = brush_instance.tile_brush_index;
											m_selected_brush.tile_brush = &m_selected_brush.tileset->brushes.at(selected_index);
											m_selected_brush.is_picking_from_layout = false;
											m_selected_brush.was_picked_from_layout = true;
											m_selected_brush.flip_x = brush_instance.is_flipped_horizontally;
											m_selected_brush.flip_y = brush_instance.is_flipped_vertically;
										}
										else
										{
											m_selected_brush.Clear();
										}
										has_just_selected_item = true;
									}
									else if (m_selected_brush.HasSelection())
									{
										for (float x = std::min(start_grid_pos.x, end_grid_pos.x); x <= std::max(start_grid_pos.x, end_grid_pos.x); ++x)
										{
											for (float y = std::min(start_grid_pos.y, end_grid_pos.y); y <= std::max(start_grid_pos.y, end_grid_pos.y); ++y)
											{
												const int drag_grid_ref = static_cast<int>((y * m_selected_brush.tile_layer->tile_layout->layout_width) + x);
												auto& tile = m_selected_brush.tile_layer->tile_layout->tile_brush_instances.at(drag_grid_ref);
												tile.tile_brush_index = m_selected_brush.tile_brush->brush_index;
												tile.is_flipped_horizontally = m_selected_brush.flip_x;
												tile.is_flipped_vertically = m_selected_brush.flip_y;
											}
										}
										m_selected_brush.tile_layer->tile_layout->BlitTileInstancesFromBrushInstances();
										m_render_from_edit = true;
										m_selected_brush.dragging_start_ref.reset();
									}
								}

							}
						}

						if (m_selected_brush.IsPickingFromLayout() == false && (m_selected_brush.tile_brush != nullptr && m_selected_brush.tile_brush->texture == nullptr))
						{
							m_selected_brush.Clear();
						}

						if (m_selected_brush.HasSelection())
						{
							if (ImGui::IsKeyPressed(ImGuiKey_R))
							{
								m_selected_brush.flip_x = !m_selected_brush.flip_x;
							}

							if (ImGui::IsKeyPressed(ImGuiKey_F))
							{
								m_selected_brush.flip_y = !m_selected_brush.flip_y;
							}
						}

						if (ImGui::IsPopupOpen("obj_popup") == false && m_request_open_obj_popup == false)
						{
							if (m_working_game_obj)
							{
								if (m_working_game_obj->initial_drag_offset.has_value() == false)
								{
									m_working_game_obj.reset();
								}
								else
								{
									if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::IsMouseClicked(ImGuiMouseButton_Left) == false)
									{
										const ImVec2 pos = ((ImGui::GetMousePos() - screen_origin) / m_zoom) - *m_working_game_obj->initial_drag_offset - m_working_game_obj->destination->sprite_pos_offset;
										m_working_game_obj->game_obj.obj_definition.x_pos = static_cast<Uint16>(pos.x / m_grid_snap) * m_grid_snap;
										m_working_game_obj->game_obj.obj_definition.y_pos = static_cast<Uint16>(pos.y / m_grid_snap) * m_grid_snap;

										if (ImGui::IsKeyPressed(ImGuiKey_R))
										{
											m_working_game_obj->game_obj.obj_definition.flip_x = !m_working_game_obj->game_obj.obj_definition.flip_x;
										}

										if (ImGui::IsKeyPressed(ImGuiKey_F))
										{
											m_working_game_obj->game_obj.obj_definition.flip_y = !m_working_game_obj->game_obj.obj_definition.flip_y;
										}

										ImGui::SetCursorPos(origin + (m_working_game_obj->game_obj.GetSpriteDrawPos() * m_zoom));
										const ImVec2 uv_min{ m_working_game_obj->game_obj.obj_definition.flip_x ? 1.0f : 0.0f, m_working_game_obj->game_obj.obj_definition.flip_y ? 1.0f : 0.0f };
										const ImVec2 uv_max{ m_working_game_obj->game_obj.obj_definition.flip_x ? 0.0f : 1.0f, m_working_game_obj->game_obj.obj_definition.flip_y ? 0.0f : 1.0f };

										if (m_working_game_obj->destination->ui_sprite)
										{
											ImGui::Image((ImTextureID)m_working_game_obj->destination->ui_sprite->texture.get(), m_working_game_obj->game_obj.dimensions * m_zoom, uv_min, uv_max, ImVec4{ 1.0f,1.0f,1.0f,0.55f });
										}
										else
										{
											ImGui::Dummy(m_working_game_obj->game_obj.dimensions * m_zoom);
											ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImGui::GetColorU32(ImVec4{ 0.0f,1.0f,0.0f,1.0f }));
										}
									}
									else if (m_working_game_obj->initial_drag_offset)
									{
										m_request_open_obj_popup = true;
										m_working_game_obj->initial_drag_offset.reset();
									}
								}
							}

							if (m_working_flipper)
							{
								if (m_working_flipper->initial_drag_offset.has_value() == false)
								{
									m_working_flipper.reset();
								}
								else
								{
									if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
									{
										if (ImGui::IsKeyPressed(ImGuiKey_R))
										{
											m_working_flipper->flipper_obj.is_x_flipped = !m_working_flipper->flipper_obj.is_x_flipped;
										}

										const ImVec2 pos = ((ImGui::GetMousePos() - screen_origin) / m_zoom) - *m_working_flipper->initial_drag_offset - m_working_flipper->destination->GetDrawPosOffset();
										m_working_flipper->flipper_obj.x_pos = static_cast<Uint16>(pos.x / m_grid_snap) * m_grid_snap;
										m_working_flipper->flipper_obj.y_pos = static_cast<Uint16>(pos.y / m_grid_snap) * m_grid_snap;

										ImGui::SetCursorPos(origin + (ImVec2{ static_cast<float>(m_working_flipper->flipper_obj.x_pos + m_working_flipper->flipper_obj.GetDrawPosOffset().x), static_cast<float>(m_working_flipper->flipper_obj.y_pos + m_working_flipper->flipper_obj.GetDrawPosOffset().y) } *m_zoom));
										const ImVec2 uv_min{ m_working_flipper->flipper_obj.is_x_flipped ? 1.0f : 0.0f, 0.0f };
										const ImVec2 uv_max{ m_working_flipper->flipper_obj.is_x_flipped ? 0.0f : 1.0f, 1.0f };
										ImGui::Image((ImTextureID)m_flipper_preview.texture.get(), m_working_flipper->flipper_obj.dimensions * m_zoom, uv_min, uv_max, ImVec4{ 1.0f,1.0f,1.0f,0.55f });
									}
									else if (m_working_flipper->initial_drag_offset)
									{
										m_working_flipper->initial_drag_offset.reset();
										*m_working_flipper->destination = m_working_flipper->flipper_obj;
										m_working_flipper->destination->SaveToROM(m_owning_ui.GetROM());
										m_render_from_edit = true;
										m_working_flipper.reset();
									}
								}
							}

							if (m_working_ring)
							{
								if (m_working_ring->initial_drag_offset.has_value() == false)
								{
									m_working_ring.reset();
								}
								else
								{
									if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
									{
										const ImVec2 pos = ((ImGui::GetMousePos() - screen_origin) / m_zoom) - *m_working_ring->initial_drag_offset - m_working_ring->destination->draw_pos_offset;
										m_working_ring->ring_obj.x_pos = static_cast<Uint16>(pos.x / m_grid_snap) * m_grid_snap;
										m_working_ring->ring_obj.y_pos = static_cast<Uint16>(pos.y / m_grid_snap) * m_grid_snap;

										ImGui::SetCursorPos(origin + (ImVec2{ static_cast<float>(m_working_ring->ring_obj.x_pos + m_working_ring->ring_obj.draw_pos_offset.x), static_cast<float>(m_working_ring->ring_obj.y_pos + m_working_ring->ring_obj.draw_pos_offset.y) } *m_zoom));
										ImGui::Image((ImTextureID)m_ring_preview.texture.get(), m_working_ring->ring_obj.dimensions * m_zoom, ImVec2{ 0,0 }, ImVec2{ 1,1 }, ImVec4{ 1.0f,1.0f,1.0f,0.55f });

									}
									else if (m_working_ring->initial_drag_offset)
									{
										m_working_ring->initial_drag_offset.reset();
										*m_working_ring->destination = m_working_ring->ring_obj;
										m_working_ring->destination->SaveToROM(m_owning_ui.GetROM());
										m_render_from_edit = true;
										m_working_ring.reset();
									}
								}
							}

							if (current_layer_settings.hover_game_objects)
							{
								for (std::unique_ptr<UIGameObject>& game_obj : m_game_object_manager.game_objects)
								{
									if (game_obj->obj_definition.instance_id == 0)
									{
										continue;
									}

									ImGui::SetCursorPos(origin + (game_obj->GetSpriteDrawPos() * m_zoom));
									ImGui::Dummy(game_obj->dimensions * m_zoom);
									if (ImGui::IsMouseHoveringRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()))
									{
										ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImGui::GetColorU32(ImVec4{ 0,192,0,255 }), 1.0f, 0, 2);

										if (current_layer_settings.visibility_culling)
										{
											rom::AnimatedObjectCullingTable anim_obj_table = rom::AnimatedObjectCullingTable::LoadFromROM(m_owning_ui.GetROM(), m_owning_ui.GetROM().ReadUint32(m_level->m_data_offsets.camera_activation_sector_anim_obj_ids));
											for (Uint32 sector_index = 0; sector_index < anim_obj_table.cells.size() - 1; ++sector_index)
											{
												const rom::AnimatedObjectCullingCell& cell = anim_obj_table.cells[sector_index];
												for (const Uint8 obj_id : cell.obj_instance_ids)
												{
													if (obj_id == game_obj->obj_definition.instance_id)
													{
														ImGui::GetWindowDrawList()->AddRect(screen_origin + (cell.bbox.min * m_zoom), screen_origin + (cell.bbox.max * m_zoom),
															ImGui::GetColorU32(ImVec4{ 255,255,255,255 }), 0, ImDrawFlags_None, 1.0f);
													}
												}
											}
										}

										if (current_layer_settings.collision && game_obj->obj_definition.collision != nullptr)
										{
											rom::CollisionSpline spline = *game_obj->obj_definition.collision;
											spline.spline_vector.min.x = static_cast<Uint16>(static_cast<Uint16>(spline.spline_vector.min.x) + game_obj->obj_definition.x_pos);
											spline.spline_vector.min.y = static_cast<Uint16>(static_cast<Uint16>(spline.spline_vector.min.y) + game_obj->obj_definition.y_pos);
											spline.spline_vector.max.x = static_cast<Uint16>(static_cast<Uint16>(spline.spline_vector.max.x) + game_obj->obj_definition.x_pos);
											spline.spline_vector.max.y = static_cast<Uint16>(static_cast<Uint16>(spline.spline_vector.max.y) + game_obj->obj_definition.y_pos);

											if (game_obj->obj_definition.flip_x)
											{
												std::swap(spline.spline_vector.min.x, spline.spline_vector.max.x);
											}

											if (game_obj->obj_definition.flip_y)
											{
												std::swap(spline.spline_vector.min.y, spline.spline_vector.max.y);
											}
											DrawCollisionSpline(spline, origin, screen_origin, current_layer_settings, false, true);
										}

										if (current_layer_settings.collision_culling && m_level->m_data_offsets.collision_tile_obj_ids.offset != 0)
										{
											rom::GameObjectCullingTable game_obj_table = rom::GameObjectCullingTable::LoadFromROM(m_owning_ui.GetROM(), m_level->m_data_offsets.collision_tile_obj_ids.offset);
											for (Uint32 sector_index = 0; sector_index < game_obj_table.cells.size() - 1; ++sector_index)
											{
												const rom::GameObjectCullingCell& cell = game_obj_table.cells[sector_index];
												for (const Uint16 obj_id : cell.obj_instance_ids)
												{
													if (obj_id == game_obj->obj_definition.instance_id)
													{
														ImGui::GetWindowDrawList()->AddRect(screen_origin + (cell.bbox.min * m_zoom), screen_origin + (cell.bbox.max * m_zoom),
															ImGui::GetColorU32(ImVec4{ 255,0,255,255 }), 0, ImDrawFlags_None, 2.0f);
													}
												}
											}
										}

										if (m_working_game_obj.has_value() == false)
										{
											if (IsDraggingObject() == false && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
											{
												m_request_open_obj_popup = true;
												m_working_game_obj.emplace();
												m_working_game_obj->destination = game_obj.get();
												m_working_game_obj->game_obj = *game_obj;
											}
											else if (IsDraggingObject() == false && IsObjectPopupOpen() == false && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
											{
												m_working_game_obj.emplace();
												m_working_game_obj->destination = game_obj.get();
												m_working_game_obj->game_obj = *game_obj;
												m_working_game_obj->initial_drag_offset = ((ImGui::GetMousePos() - screen_origin) / m_zoom) - m_working_game_obj->game_obj.GetSpriteDrawPos();
											}
											else if (current_layer_settings.hover_game_objects_tooltip && ImGui::BeginTooltip())
											{
												ImGui::SeparatorText("Game Object");
												if (game_obj->ui_sprite)
												{
													ImGui::Text("Sprite Table: 0x%04X", game_obj->sprite_table_address);
													const ImVec2 uv_min{ game_obj->obj_definition.FlipX() ? 1.0f : 0.0f, game_obj->obj_definition.FlipY() ? 1.0f : 0.0f };
													const ImVec2 uv_max{ game_obj->obj_definition.FlipX() ? 0.0f : 1.0f, game_obj->obj_definition.FlipY() ? 0.0f : 1.0f };
													ImGui::Image((ImTextureID)game_obj->ui_sprite->texture.get(), ImVec2(static_cast<float>(game_obj->ui_sprite->texture->w) * 2.0f, static_cast<float>(game_obj->ui_sprite->texture->h) * 2.0f), uv_min, uv_max);
												}
												else
												{
													ImGui::Text("<No Sprite>");
												}

												ImGui::Text("Type ID: 0x%02X", game_obj->obj_definition.type_id);
												ImGui::Text("Instance ID: 0x%02X", game_obj->obj_definition.instance_id);
												ImGui::Text("Unk 1: %d", game_obj->obj_definition.unk_1);
												ImGui::Text("Unk 2: %d", game_obj->obj_definition.unk_2);
												ImGui::Text("X: 0x%04X", game_obj->obj_definition.x_pos);
												ImGui::Text("Y: 0x%04X", game_obj->obj_definition.y_pos);
												ImGui::Text("Width: 0x%04X", game_obj->obj_definition.collision_width);
												ImGui::Text("Height: 0x%04X", game_obj->obj_definition.collision_height);
												ImGui::Text("Anim Definition ptr: 0x%08X", game_obj->obj_definition.animation_definition);
												ImGui::Text("Collision Bbox ptr: 0x%08X", game_obj->obj_definition.collision_bbox_ptr);
												ImGui::Text("Flags: 0x%04X", game_obj->obj_definition.flags);
												ImGui::Text("Flip X: %d", game_obj->obj_definition.FlipX());
												ImGui::Text("Flip Y: %d", game_obj->obj_definition.FlipY());

												ImGui::EndTooltip();
											}
										}
									}
								}

								if (m_level != nullptr)
								{
									for (rom::FlipperInstance& flipper_obj : m_level->m_flipper_instances)
									{
										const ImVec2 flipper_realpos{ static_cast<float>(flipper_obj.x_pos + flipper_obj.GetDrawPosOffset().x), static_cast<float>(flipper_obj.y_pos + flipper_obj.GetDrawPosOffset().y) };
										const ImVec2 flipper_dimensions{ rom::FlipperInstance::width, rom::FlipperInstance::height };

										ImGui::SetCursorPos(origin + (flipper_realpos * m_zoom));
										ImGui::Dummy(flipper_dimensions * m_zoom);
										if (ImGui::IsMouseHoveringRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()))
										{
											ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImGui::GetColorU32(ImVec4{ 0,192,0,255 }), 1.0f, 0, 2);

											//if (IsDraggingObject() == false && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
											//{
											//	m_request_open_obj_popup = true;
											//	m_working_flipper.emplace();
											//	m_working_flipper->destination = &flipper_obj;
											//	m_working_flipper->flipper_obj = flipper_obj;
											//}
											//else
											if (IsDraggingObject() == false && IsObjectPopupOpen() == false && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
											{
												m_working_flipper.emplace();
												m_working_flipper->destination = &flipper_obj;
												m_working_flipper->flipper_obj = flipper_obj;
												m_working_flipper->initial_drag_offset = ((ImGui::GetMousePos() - screen_origin) / m_zoom) - flipper_realpos;
											}
											else if (current_layer_settings.hover_game_objects_tooltip && ImGui::BeginTooltip())
											{
												ImGui::SeparatorText("Flipper Object");

												ImGui::Text("X: 0x%04X", flipper_obj.x_pos);
												ImGui::Text("Y: 0x%04X", flipper_obj.y_pos);
												ImGui::Text("Flip X: %d", flipper_obj.is_x_flipped);

												ImGui::EndTooltip();
											}
										}
									}

									for (rom::RingInstance& ring_obj : m_level->m_ring_instances)
									{
										const ImVec2 ring_realpos{ static_cast<float>(ring_obj.x_pos + ring_obj.draw_pos_offset.x), static_cast<float>(ring_obj.y_pos + ring_obj.draw_pos_offset.y) };
										const ImVec2 ring_dimensions{ rom::RingInstance::width, rom::RingInstance::height };

										ImGui::SetCursorPos(origin + (ring_realpos * m_zoom));
										ImGui::Dummy(ring_dimensions * m_zoom);
										if (ImGui::IsMouseHoveringRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()))
										{
											ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImGui::GetColorU32(ImVec4{ 0,192,0,255 }), 1.0f, 0, 2);

											//if (IsDraggingObject() == false && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
											//{
											//	m_request_open_obj_popup = true;
											//	m_working_ring.emplace();
											//	m_working_ring->destination = &ring_obj;
											//	m_working_ring->flipper_obj = ring_obj;
											//}
											//else
											if (IsDraggingObject() == false && IsObjectPopupOpen() == false && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
											{
												m_working_ring.emplace();
												m_working_ring->destination = &ring_obj;
												m_working_ring->ring_obj = ring_obj;
												m_working_ring->initial_drag_offset = ((ImGui::GetMousePos() - screen_origin) / m_zoom) - ring_realpos;
											}
											else if (current_layer_settings.hover_game_objects_tooltip && ImGui::BeginTooltip())
											{
												ImGui::SeparatorText("Ring Object");

												ImGui::Text("X: 0x%04X", ring_obj.x_pos);
												ImGui::Text("Y: 0x%04X", ring_obj.y_pos);

												ImGui::EndTooltip();
											}
										}
									}
								}
							}
						}

						if (m_working_spline)
						{
							if (m_working_spline->current_mode == WorkingSplineMode::PLACING_START_POINT || m_working_spline->current_mode == WorkingSplineMode::PLACING_END_POINT)
							{
								if (m_working_spline->current_mode != WorkingSplineMode::PLACING_END_POINT)
								{
									m_working_spline->spline.spline_vector.min.x = static_cast<int>(((ImGui::GetMousePos().x - screen_origin.x) / m_zoom) / m_grid_snap) * m_grid_snap;
									m_working_spline->spline.spline_vector.min.y = static_cast<int>(((ImGui::GetMousePos().y - screen_origin.y) / m_zoom) / m_grid_snap) * m_grid_snap;
									if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
									{
										m_working_spline->current_mode = WorkingSplineMode::PLACING_END_POINT;
									}
								}
								else
								{
									if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
									{
										m_working_spline->current_mode = WorkingSplineMode::EDITING_SPLINE_PROPERTIES;
										ImGui::OpenPopup("spline_popup");
									}
								}

								m_working_spline->spline.spline_vector.max.x = static_cast<int>(((ImGui::GetMousePos().x - screen_origin.x) / m_zoom) / m_grid_snap) * m_grid_snap;
								m_working_spline->spline.spline_vector.max.y = static_cast<int>(((ImGui::GetMousePos().y - screen_origin.y) / m_zoom) / m_grid_snap) * m_grid_snap;
							}
							else if (m_working_spline->current_mode == WorkingSplineMode::DRAGGING_POINT && m_working_spline->dest_spline_point != nullptr)
							{
								if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
								{
									m_working_spline->dest_spline_point->x = static_cast<int>(((ImGui::GetMousePos().x - screen_origin.x) / m_zoom) / m_grid_snap) * m_grid_snap;
									m_working_spline->dest_spline_point->y = static_cast<int>(((ImGui::GetMousePos().y - screen_origin.y) / m_zoom) / m_grid_snap) * m_grid_snap;
								}
								else
								{
									m_working_spline->current_mode = WorkingSplineMode::EDITING_SPLINE_PROPERTIES;
									ImGui::OpenPopup("spline_popup");
									m_working_spline->dest_spline_point = nullptr;
								}
							}
						}

						if (m_working_game_obj.has_value() == false && m_working_flipper.has_value() == false && (m_working_spline.has_value() == false || m_working_spline->dest_spline_point != nullptr))
						{
							ImGui::CloseCurrentPopup();
						}

						if (m_request_open_obj_popup)
						{
							const UIGameObject& dest_obj = *m_working_game_obj->destination;
							const ImVec2 origin_pos{ screen_origin.x + (dest_obj.GetSpriteDrawPos().x * m_zoom), (screen_origin.y + dest_obj.GetSpriteDrawPos().y * m_zoom) };
							ImGui::ScrollToRect(ImGui::GetCurrentWindow(), ImRect{ origin_pos, origin_pos + (dest_obj.dimensions * m_zoom) }, ImGuiScrollFlags_KeepVisibleEdgeX | ImGuiScrollFlags_KeepVisibleEdgeY);

							ImGui::OpenPopup("obj_popup");
							m_request_open_obj_popup = false;
						}

						if (m_working_game_obj && ImGui::IsPopupOpen("obj_popup"))
						{
							ImGui::SetCursorPos((origin + m_working_game_obj->game_obj.GetSpriteDrawPos()) * m_zoom);
							const ImVec2 uv_min{ m_working_game_obj->game_obj.obj_definition.FlipX() ? 1.0f : 0.0f, m_working_game_obj->game_obj.obj_definition.FlipY() ? 1.0f : 0.0f };
							const ImVec2 uv_max{ m_working_game_obj->game_obj.obj_definition.FlipX() ? 0.0f : 1.0f, m_working_game_obj->game_obj.obj_definition.FlipY() ? 0.0f : 1.0f };
							if (m_working_game_obj->destination->ui_sprite != nullptr)
							{
								ImGui::Image((ImTextureID)m_working_game_obj->destination->ui_sprite->texture.get(), m_working_game_obj->game_obj.dimensions * m_zoom, uv_min, uv_max, ImVec4{ 1.0f,1.0f,1.0f,0.55f });
							}
							else
							{
								ImGui::Dummy(m_working_game_obj->game_obj.dimensions * m_zoom);
								ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImGui::GetColorU32(ImVec4{ 0.0f,1.0f,0.0f,1.0f }));
							}
						}

						if (m_working_game_obj && ImGui::BeginPopup("obj_popup"))
						{
							if (ImGui::Button("Confirm"))
							{
								*m_working_game_obj->destination = m_working_game_obj->game_obj;
								m_working_game_obj->destination->obj_definition.SaveToROM(m_owning_ui.GetROM());
								m_render_from_edit = true;
								m_working_game_obj.reset();
								ImGui::CloseCurrentPopup();
							}
							ImGui::SameLine();
							if (ImGui::Button("Cancel"))
							{
								m_working_game_obj.reset();
								ImGui::CloseCurrentPopup();
							}

							if (ImGui::Button("Create duplicate here"))
							{
								if (UIGameObject* new_obj = m_game_object_manager.DuplicateGameObject(m_working_game_obj->game_obj, m_level->m_data_offsets))
								{
									new_obj->had_collision_sectors_on_rom = true;
									new_obj->had_culling_sectors_on_rom = true;
									new_obj->obj_definition.SaveToROM(m_owning_ui.GetROM());
									m_render_from_edit = true;
								}
								m_working_game_obj.reset();
								ImGui::CloseCurrentPopup();
							}
							ImGui::SameLine();
							if (ImGui::Button("Delete"))
							{
								if (UIGameObject* removed_obj = m_game_object_manager.DeleteGameObject(m_working_game_obj->game_obj))
								{
									removed_obj->obj_definition.SaveToROM(m_owning_ui.GetROM());
									m_render_from_edit = true;
								}
								m_working_game_obj.reset();
								ImGui::CloseCurrentPopup();
							}
							ImGui::SameLine();
							if (m_working_game_obj)
							{
								ImGui::BeginDisabled(m_working_game_obj->destination->had_collision_sectors_on_rom && m_working_game_obj->destination->had_culling_sectors_on_rom);
								if (ImGui::Button("Enable culling"))
								{
									m_working_game_obj->destination->had_collision_sectors_on_rom = true;
									m_working_game_obj->destination->had_culling_sectors_on_rom = true;
									m_working_game_obj->destination->obj_definition.SaveToROM(m_owning_ui.GetROM());
									m_working_game_obj.reset();
								}
								ImGui::EndDisabled();
							}

							if (m_working_game_obj)
							{
								const Point origin_offset{ m_working_game_obj->game_obj.obj_definition.x_pos,m_working_game_obj->game_obj.obj_definition.y_pos };
								int pos[2] = { static_cast<int>(origin_offset.x), static_cast<int>(origin_offset.y) };
								if (ImGui::InputInt2("Object Pos", pos))
								{
									m_working_game_obj->game_obj.obj_definition.x_pos = pos[0];
									m_working_game_obj->game_obj.obj_definition.y_pos = pos[1];
								}
							}
							ImGui::EndPopup();
						}

						if (ImGui::BeginPopup("spline_popup"))
						{
							if (m_working_spline.has_value() == false || m_working_spline->current_mode != WorkingSplineMode::EDITING_SPLINE_PROPERTIES)
							{
								ImGui::CloseCurrentPopup();
							}
							else
							{
								if (ImGui::Button("Confirm"))
								{
									if (m_working_spline->spline.IsRadial())
									{
										const bool is_max_point = true;
										const Point offset = is_max_point ? Point{ m_working_spline->spline.spline_vector.min - m_working_spline->destination->spline_vector.min } : Point{ m_working_spline->spline.spline_vector.max - m_working_spline->destination->spline_vector.max };
										const Uint16 spline_target_id = m_working_spline->spline.instance_id_binding;

										if (m_working_spline->spline.IsRing() && m_working_spline->spline.instance_id_binding != 0)
										{
											auto target_ring_obj = std::find_if(std::begin(m_level->m_ring_instances), std::end(m_level->m_ring_instances),
												[spline_target_id](const rom::RingInstance& ring_obj)
												{
													return ring_obj.instance_id == spline_target_id;
												});

											if (target_ring_obj != std::end(m_level->m_ring_instances))
											{
												target_ring_obj->x_pos += offset.x;
												target_ring_obj->y_pos += offset.y;
												m_render_from_edit = true;
											}
										}
										else if (m_working_spline->spline.IsRadial() && m_working_spline->spline.instance_id_binding != 0)
										{
											auto target_game_obj = std::find_if(std::begin(m_game_object_manager.game_objects), std::end(m_game_object_manager.game_objects),
												[spline_target_id](const std::unique_ptr<UIGameObject>& ui_game_obj)
												{
													return static_cast<Uint16>(ui_game_obj->obj_definition.instance_id) == spline_target_id;
												});

											if (target_game_obj != std::end(m_game_object_manager.game_objects))
											{
												target_game_obj->get()->obj_definition.x_pos += offset.x;
												target_game_obj->get()->obj_definition.y_pos += offset.y;
												target_game_obj->get()->obj_definition.SaveToROM(m_owning_ui.GetROM());
												m_render_from_edit = true;
											}
										}
									}

									if (m_working_spline->destination != nullptr)
									{
										*m_working_spline->destination = m_working_spline->spline;
									}
									else
									{
										m_spline_manager.splines.emplace_back(std::move(m_working_spline->spline));
									}
									m_working_spline.reset();
									ImGui::CloseCurrentPopup();
								}
								ImGui::SameLine();
								if (ImGui::Button("Cancel"))
								{
									m_working_spline.reset();
									ImGui::CloseCurrentPopup();
								}
								ImGui::SameLine();
								if (ImGui::Button("Delete"))
								{
									auto found_spline_it = std::find_if(std::begin(m_spline_manager.splines), std::end(m_spline_manager.splines),
										[this](const rom::CollisionSpline& spline)
										{
											return &spline == m_working_spline->destination;
										});
									m_spline_manager.splines.erase(found_spline_it);
									m_working_spline.reset();
									ImGui::CloseCurrentPopup();
								}

								if (m_working_spline)
								{
									int flags_working_info = m_working_spline->spline.object_type_flags;
									ImGui::InputInt("Obj Flags: 0x%04X", &flags_working_info, 1, 1, ImGuiInputTextFlags_CharsHexadecimal);
									m_working_spline->spline.object_type_flags = (0x0000FFFF & flags_working_info);

									int working_info = m_working_spline->spline.instance_id_binding;
									ImGui::InputInt("Instance ID Binding: 0x%04X", &working_info, 1, 1, ImGuiInputTextFlags_CharsHexadecimal);
									m_working_spline->spline.instance_id_binding = (0x0000FFFF & working_info);

									int spline_min[2] = { static_cast<int>(m_working_spline->spline.spline_vector.min.x), static_cast<int>(m_working_spline->spline.spline_vector.min.y) };
									int spline_max[2] = { static_cast<int>(m_working_spline->spline.spline_vector.max.x), static_cast<int>(m_working_spline->spline.spline_vector.max.y) };

									ImGui::InputInt2("Spline Min", spline_min);
									ImGui::InputInt2("Spline Max", spline_max);

									m_working_spline->spline.spline_vector.min.x = spline_min[0];
									m_working_spline->spline.spline_vector.min.y = spline_min[1];
									m_working_spline->spline.spline_vector.max.x = spline_max[0];
									m_working_spline->spline.spline_vector.max.y = spline_max[1];

									bool teleporter = m_working_spline->spline.IsTeleporter();
									if (ImGui::Checkbox("Teleporter", &teleporter))
									{
										if (teleporter)
										{
											m_working_spline->spline.object_type_flags |= 0x1000;
										}
										else
										{
											m_working_spline->spline.object_type_flags &= ~0x1000;
										}
									}

									bool ring = m_working_spline->spline.IsRing();
									if (ImGui::Checkbox("Ring", &ring))
									{
										if (ring)
										{
											m_working_spline->spline.object_type_flags |= 0x2000;
										}
										else
										{
											m_working_spline->spline.object_type_flags &= ~0x2000;
										}
									}

									bool radial_collision = m_working_spline->spline.IsRadial();
									if (ImGui::Checkbox("Radial Collision", &radial_collision))
									{
										if (radial_collision)
										{
											m_working_spline->spline.object_type_flags |= 0x8000;
										}
										else
										{
											m_working_spline->spline.object_type_flags &= ~0x8000;
										}
									}

									bool can_walk = m_working_spline->spline.IsWalkable();
									if (ImGui::Checkbox("Can walk", &can_walk))
									{
										if (can_walk)
										{
											m_working_spline->spline.object_type_flags |= 0x0080;
										}
										else
										{
											m_working_spline->spline.object_type_flags &= ~0x0080;
										}
									}

									bool trigger_slide = m_working_spline->spline.IsSlippery();
									if (ImGui::Checkbox("Slippery", &trigger_slide))
									{
										if (trigger_slide)
										{
											m_working_spline->spline.object_type_flags |= 0x0100;
										}
										else
										{
											m_working_spline->spline.object_type_flags &= ~0x0100;
										}
									}

									bool standard_bumper = m_working_spline->spline.IsBumper();
									if (ImGui::Checkbox("Bumper", &standard_bumper))
									{
										if (standard_bumper)
										{
											m_working_spline->spline.object_type_flags |= 0x4000;
										}
										else
										{
											m_working_spline->spline.object_type_flags &= ~0x4000;
										}
									}
								}
							}
							ImGui::EndPopup();
						}

						if (is_hovering_tile_area && has_just_selected_item == false && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && IsEditingSomething() == false && IsObjectPopupOpen() == false)
						{
							ImGui::OpenPopup("create_new");
						}

						if (m_working_spline && (m_working_spline->current_mode == WorkingSplineMode::NONE || (ImGui::IsPopupOpen("spline_popup") == false && m_working_spline.has_value() && (m_working_spline->current_mode == WorkingSplineMode::EDITING_SPLINE_PROPERTIES))))
						{
							m_working_spline.reset();
						}
					}

					if (tile_area != nullptr)
					{
						static bool is_dragging = false;
						static ImVec2 drag_start_pos = ImGui::GetMousePos();
						static ImVec2 scroll_start_pos = ImGui::GetCurrentContext()->CurrentWindow->Scroll;

						if (ImGui::IsMouseDown(ImGuiMouseButton_Middle))
						{
							if (is_dragging == false && is_hovering_tile_area && ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
							{
								drag_start_pos = ImGui::GetMousePos();
								scroll_start_pos = ImGui::GetCurrentContext()->CurrentWindow->Scroll;
								is_dragging = true;
							}

							if (is_dragging)
							{
								tile_area->Scroll = scroll_start_pos + (drag_start_pos - ImGui::GetMousePos());
							}
						}
						else
						{
							is_dragging = false;
						}

						int scroll_multiplier = 1;

						if (is_hovering_tile_area && ImGui::IsKeyDown(ImGuiKey_ModCtrl))
						{
							m_zoom += ImGui::GetIO().MouseWheel / 10.0f;
						}

						if (ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift))
						{
							scroll_multiplier = 2;
						}

						const int scroll_amount = 4 * scroll_multiplier;

						if (ImGui::IsKeyDown(ImGuiKey_W) || ImGui::IsKeyDown(ImGuiKey_UpArrow))
						{
							tile_area->Scroll.y -= scroll_amount;
						}

						if (ImGui::IsKeyDown(ImGuiKey_A) || ImGui::IsKeyDown(ImGuiKey_LeftArrow))
						{
							tile_area->Scroll.x -= scroll_amount;
						}

						if (ImGui::IsKeyDown(ImGuiKey_S) || ImGui::IsKeyDown(ImGuiKey_DownArrow))
						{
							tile_area->Scroll.y += scroll_amount;
						}

						if (ImGui::IsKeyDown(ImGuiKey_D) || ImGui::IsKeyDown(ImGuiKey_RightArrow))
						{
							tile_area->Scroll.x += scroll_amount;
						}

						if (ImGui::BeginPopup("create_new"))
						{
							if (ImGui::MenuItem("Create spline"))
							{
								m_working_spline.emplace();
								m_working_spline->current_mode = WorkingSplineMode::PLACING_START_POINT;
								m_working_spline->spline = {};
								ImGui::CloseCurrentPopup();
							}
							ImGui::EndPopup();
						}

					}

				}
				ImGui::EndChild();
			}
			ImGui::EndGroup();

			if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
			{
				if (m_selected_brush.IsActive())
				{
					if (m_selected_brush.was_picked_from_layout == true)
					{
						m_selected_brush.was_picked_from_layout = false;
						m_selected_brush.is_picking_from_layout = true;
						m_selected_brush.tile_brush = nullptr;
						m_selected_brush.dragging_start_ref.reset();
					}
					else
					{
						m_selected_brush.Clear();
					}
				}

				if (m_selected_tile.IsActive())
				{
					if (m_selected_tile.was_picked_from_layout == true)
					{
						m_selected_tile.was_picked_from_layout = false;
						m_selected_tile.is_picking_from_layout = true;
						m_selected_tile.tile_selection = nullptr;
						m_selected_tile.dragging_start_ref.reset();
					}
					else
					{
						m_selected_tile.Clear();
					}
				}
			}
		}
		ImGui::End();
	}

	void EditorTileLayoutViewer::DrawLevelInfo()
	{
		char working_buffer[256];

		sprintf_s(working_buffer, "%s", m_level->m_level_name.c_str());
		ImGui::InputText("Level Name", working_buffer, std::size(working_buffer), ImGuiInputTextFlags_EnterReturnsTrue);
		int emerald_count = m_owning_ui.GetROM().ReadUint8(m_level->m_data_offsets.emerald_count);
		ImGui::InputInt("Emeralds", &emerald_count);
		int flipper_count = m_owning_ui.GetROM().ReadUint8(m_owning_ui.GetROM().ReadUint32(m_level->m_data_offsets.flipper_count));
		ImGui::InputInt("Flippers", &flipper_count);
		int ring_count = m_owning_ui.GetROM().ReadUint8(m_owning_ui.GetROM().ReadUint32(m_level->m_data_offsets.ring_count));
		ImGui::InputInt("Rings", &ring_count);
		int music_id = m_owning_ui.GetROM().ReadUint8(m_level->m_data_offsets.music_id);
		ImGui::InputInt("Music Bank", &music_id, 1, 1, ImGuiInputTextFlags_CharsHexadecimal);
		int player_start_x = m_owning_ui.GetROM().ReadUint16(m_level->m_data_offsets.player_start_position_x);
		int player_start_y = m_owning_ui.GetROM().ReadUint16(m_level->m_data_offsets.player_start_position_y);
		int player_start[2] = { player_start_x, player_start_y };
		ImGui::InputInt2("Player Start Pos", player_start, ImGuiInputTextFlags_CharsHexadecimal);
		int level_width = m_owning_ui.GetROM().ReadUint8(m_level->m_data_offsets.tile_layout_width);
		int level_height = m_owning_ui.GetROM().ReadUint8(m_level->m_data_offsets.tile_layout_height);
		int level_dimensions[2] = { level_width * 128, level_height * 128 };
		int level_dimensions_tiles[2] = { level_width, level_height };
		ImGui::InputInt2("Dimensions", level_dimensions);
		ImGui::InputInt2("Dim. 128x128 tiles", level_dimensions_tiles);
	}

	void EditorTileLayoutViewer::DrawObjectTable()
	{
		if (ImGui::BeginChild("Object Table"))
		{
			if (ImGui::BeginTable("GameObjects", 4, ImGuiTableFlags_RowBg))
			{
				ImGui::TableNextColumn();
				ImGui::TableHeader("Instance ID");
				ImGui::TableNextColumn();
				ImGui::TableHeader("Type ID");
				ImGui::TableNextColumn();
				ImGui::TableHeader("X");
				ImGui::TableNextColumn();
				ImGui::TableHeader("Y");

				const std::unique_ptr<UIGameObject>* previous_game_object = nullptr;

				for (const std::unique_ptr<UIGameObject>& game_object : m_game_object_manager.game_objects)
				{
					if (game_object->obj_definition.instance_id == 0)
					{
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 192, 0, 192));
					}
					const bool is_hovered = (ImGui::TableGetHoveredRow() == ImGui::TableGetRowIndex());
					const bool is_selected = (previous_game_object && m_working_game_obj && m_working_game_obj->destination == previous_game_object->get());
					if (is_hovered || is_selected)
					{
						ImGui::PushStyleColor(ImGuiCol_TableRowBg, ImVec4(0.0f, 0.5f, 0.0f, 1.0f));
						ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, ImVec4(0.0f, 0.3f, 0.0f, 1.0f));

						if (is_selected)
						{
							ImGui::ScrollToItem(ImGuiScrollFlags_KeepVisibleCenterY);
						}

						if (previous_game_object && is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
						{
							m_request_open_obj_popup = true;
							m_working_game_obj.emplace();
							m_working_game_obj->destination = previous_game_object->get();
							m_working_game_obj->game_obj = **previous_game_object;
						}
					}

					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::Text("0x%02X", game_object->obj_definition.instance_id);
					ImGui::TableNextColumn();
					ImGui::Text("0x%02X", game_object->obj_definition.type_id);
					ImGui::TableNextColumn();
					ImGui::Text("0x%04X", game_object->obj_definition.x_pos);
					ImGui::TableNextColumn();
					ImGui::Text("0x%04X", game_object->obj_definition.y_pos);
					if (is_hovered || is_selected)
					{
						ImGui::PopStyleColor(2);
					}
					if (game_object->obj_definition.instance_id == 0)
					{
						ImGui::PopStyleColor();
					}

					previous_game_object = &game_object;
				}

				ImGui::EndTable();
			}
		}
		ImGui::EndChild();
	}

	void EditorTileLayoutViewer::DrawRingsTable()
	{
		if (m_level == nullptr)
		{
			return;
		}

		if (ImGui::BeginChild("Rings Table"))
		{
			if (ImGui::BeginTable("Rings", 3))
			{
				ImGui::TableNextColumn();
				ImGui::TableHeader("Instance ID");
				ImGui::TableNextColumn();
				ImGui::TableHeader("X");
				ImGui::TableNextColumn();
				ImGui::TableHeader("Y");

				for (const rom::RingInstance& ring : m_level->m_ring_instances)
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::Text("0x%02X", ring.instance_id);
					ImGui::TableNextColumn();
					ImGui::Text("0x%04X", ring.x_pos);
					ImGui::TableNextColumn();
					ImGui::Text("0x%04X", ring.y_pos);
				}

				ImGui::EndTable();
			}
		}
		ImGui::EndChild();
	}

	void EditorTileLayoutViewer::DrawFlippersTable()
	{
		if (m_level == nullptr)
		{
			return;
		}

		if (ImGui::BeginChild("Flippers Table"))
		{
			if (ImGui::BeginTable("Flippers", 3))
			{
				ImGui::TableNextColumn();
				ImGui::TableHeader("Flip X");
				ImGui::TableNextColumn();
				ImGui::TableHeader("X");
				ImGui::TableNextColumn();
				ImGui::TableHeader("Y");

				for (const rom::FlipperInstance& flipper : m_level->m_flipper_instances)
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::Text("%s", flipper.is_x_flipped ? "true" : "false");
					ImGui::TableNextColumn();
					ImGui::Text("0x%04X", flipper.x_pos);
					ImGui::TableNextColumn();
					ImGui::Text("0x%04X", flipper.y_pos);
				}

				ImGui::EndTable();
			}
		}
		ImGui::EndChild();
	}

	void EditorTileLayoutViewer::PrepareRenderRequest(RenderRequestType render_request)
	{
		switch (render_request)
		{
			case RenderRequestType::NONE:
				break;

			case RenderRequestType::LEVEL:
			{
				{
					const auto& buffer = m_owning_ui.GetROM().m_buffer;
					const Uint32 BGTilesetOffsets = m_owning_ui.GetROM().ReadUint32(m_level->m_data_offsets.background_tileset);
					const Uint32 BGTilesetLayouts = m_owning_ui.GetROM().ReadUint32(m_level->m_data_offsets.background_tile_layout);
					const Uint32 BGTilesetBrushes = m_owning_ui.GetROM().ReadUint32(m_level->m_data_offsets.background_tile_brushes);
					const Uint32 FGTilesetLayouts = m_owning_ui.GetROM().ReadUint32(m_level->m_data_offsets.foreground_tile_layout);


					const Uint16 LevelDimensionsX = m_owning_ui.GetROM().ReadUint16(m_level->m_data_offsets.tile_layout_width);
					const Uint16 LevelDimensionsY = m_owning_ui.GetROM().ReadUint16(m_level->m_data_offsets.tile_layout_height);

					RenderTileLayoutRequest request;

					request.tileset_address = BGTilesetOffsets;
					request.tile_brushes_address = BGTilesetBrushes;
					request.tile_brushes_address_end = FGTilesetLayouts;

					request.tile_brush_width = 4;
					request.tile_brush_height = 4;

					request.tile_layout_width = LevelDimensionsX / (rom::TileBrush<4, 4>::s_brush_width * rom::TileSet::s_tile_width);
					request.tile_layout_height = LevelDimensionsY / (rom::TileBrush<4, 4>::s_brush_height * rom::TileSet::s_tile_height);

					request.tile_layout_address = BGTilesetLayouts;
					request.tile_layout_address_end = request.tile_layout_address + (request.tile_layout_width * request.tile_layout_height * 2);

					request.is_chroma_keyed = false;
					request.compression_algorithm = CompressionAlgorithm::SSC;
					char levelname_buffer[32];
					sprintf_s(levelname_buffer, "level_%d", m_level->m_level_index);
					request.layout_type_name = levelname_buffer;
					request.layout_layout_name = "bg";

					m_working_palette_set = *rom::PaletteSet::LoadFromROM(m_owning_ui.GetROM(), m_level->m_data_offsets.palette_set);
					m_level->m_tile_layers[0].palette_set = *rom::PaletteSet::LoadFromROM(m_owning_ui.GetROM(), m_level->m_data_offsets.palette_set);
					request.store_tileset = &m_level->m_tile_layers[0].tileset;
					request.store_layout = &m_level->m_tile_layers[0].tile_layout;

					m_tile_layout_render_requests.emplace_back(std::move(request));
				}

				{
					const auto& buffer = m_owning_ui.GetROM().m_buffer;
					const rom::LevelDataOffsets level_data_offsets{ m_level->m_level_index };
					const Uint32 FGTilesetOffsets = m_owning_ui.GetROM().ReadUint32(level_data_offsets.foreground_tileset);
					const Uint32 FGTilesetLayouts = m_owning_ui.GetROM().ReadUint32(level_data_offsets.foreground_tile_layout);
					const Uint32 FGTilesetBrushes = m_owning_ui.GetROM().ReadUint32(level_data_offsets.foreground_tile_brushes);
					const Uint32 BGTilesetBrushes = m_owning_ui.GetROM().ReadUint32(level_data_offsets.background_tile_brushes);


					const Uint16 LevelDimensionsX = m_owning_ui.GetROM().ReadUint16(level_data_offsets.tile_layout_width);
					const Uint16 LevelDimensionsY = m_owning_ui.GetROM().ReadUint16(level_data_offsets.tile_layout_height);

					RenderTileLayoutRequest request;

					request.tileset_address = FGTilesetOffsets;
					request.tile_brushes_address = FGTilesetBrushes;
					request.tile_brushes_address_end = BGTilesetBrushes;

					request.tile_brush_width = 4;
					request.tile_brush_height = 4;

					request.tile_layout_width = LevelDimensionsX / (rom::TileBrush<4, 4>::s_brush_width * rom::TileSet::s_tile_width);
					request.tile_layout_height = LevelDimensionsY / (rom::TileBrush<4, 4>::s_brush_height * rom::TileSet::s_tile_height);

					request.tile_layout_address = FGTilesetLayouts;
					request.tile_layout_address_end = request.tile_layout_address + (request.tile_layout_width * request.tile_layout_height * 2);

					request.is_chroma_keyed = true;
					request.compression_algorithm = CompressionAlgorithm::SSC;

					char levelname_buffer[32];
					sprintf_s(levelname_buffer, "level_%d", m_level->m_level_index);
					request.layout_type_name = levelname_buffer;
					request.layout_layout_name = "fg";

					m_level->m_tile_layers[1].palette_set = *rom::PaletteSet::LoadFromROM(m_owning_ui.GetROM(), m_level->m_data_offsets.palette_set);
					request.store_tileset = &m_level->m_tile_layers[1].tileset;
					request.store_layout = &m_level->m_tile_layers[1].tile_layout;;

					m_tile_layout_render_requests.emplace_back(std::move(request));
				}
			}
			break;

			case RenderRequestType::OPTIONS:
			{
				Reset();

				RenderTileLayoutRequest request;

				request.tileset_address = rom::OptionsMenuTileset;
				request.tile_brushes_address = rom::OptionsMenuTileBrushes;
				request.tile_brushes_address_end = 0x000BE1BC;
				request.tile_layout_address = rom::OptionsMenuTileLayout;
				request.tile_layout_address_end = 0x000BE248;

				request.tile_brush_width = 4;
				request.tile_brush_height = 4;

				request.tile_layout_width = 0xA;
				request.tile_layout_height = 0x7;

				request.is_chroma_keyed = false;
				request.compression_algorithm = CompressionAlgorithm::SSC;

				m_working_palette_set = *m_owning_ui.GetROM().GetOptionsScreenPaletteSet();

				m_tile_layout_render_requests.emplace_back(std::move(request));
			}
			break;

			case RenderRequestType::INTRO:
			{
				{
					Reset();
					{
						RenderTileLayoutRequest request;

						request.tileset_address = rom::IntroCutscenesTileset;
						request.tile_layout_width = m_owning_ui.GetROM().ReadUint16(rom::IntroCutsceneTileLayoutSky);
						request.tile_layout_height = m_owning_ui.GetROM().ReadUint16(rom::IntroCutsceneTileLayoutSky + sizeof(Uint16));
						request.tile_layout_address = rom::IntroCutsceneTileLayoutSky;
						request.palette_line = 1;

						request.tile_brush_width = 1;
						request.tile_brush_height = 1;

						request.is_chroma_keyed = false;
						request.show_brush_previews = false;
						request.compression_algorithm = CompressionAlgorithm::LZSS;

						request.layout_type_name = "intro";
						request.layout_layout_name = "bg";

						m_working_palette_set = *m_owning_ui.GetROM().GetIntroCutscenePaletteSet();
						m_tile_layout_render_requests.emplace_back(std::move(request));
					}
				}

				{
					{
						RenderTileLayoutRequest request;

						request.tileset_address = 0x000A3124 + 2;

						// Veg-o Fortress
						request.tile_layout_width = m_owning_ui.GetROM().ReadUint16(0x000A1B8C);
						request.tile_layout_height = m_owning_ui.GetROM().ReadUint16(0x000A1B8E);
						request.tile_layout_address = 0x000A1B8C;
						request.palette_line = 1;

						// Veg-o Fortress empty topsection
						//request.tile_layout_width = m_owning_ui.GetROM().ReadUint16(0x000A2168);
						//request.tile_layout_height = m_owning_ui.GetROM().ReadUint16(0x000A216A);
						//request.tile_layout_address = 0x000A2168;
						//request.palette_line = 1;


						request.tile_brush_width = 1;
						request.tile_brush_height = 1;

						request.is_chroma_keyed = false;
						request.show_brush_previews = false;
						request.compression_algorithm = CompressionAlgorithm::LZSS;

						request.layout_type_name = "intro";
						request.layout_layout_name = "veg_o_fortress";

						m_working_palette_set = *m_owning_ui.GetROM().GetIntroCutscenePaletteSet();

						m_tile_layout_render_requests.emplace_back(std::move(request));
					}
				}
			}
			break;

			case RenderRequestType::INTRO_SHIP:
			{
				Reset();

				RenderTileLayoutRequest request;

				request.tileset_address = 0x000A3124 + 2;

				// Robotnik ship
				request.tile_layout_width = m_owning_ui.GetROM().ReadUint16(0x000a30bc);
				request.tile_layout_height = m_owning_ui.GetROM().ReadUint16(0x000a30be);
				request.tile_layout_address = 0x000a30bc;
				request.palette_line = 1;

				request.tile_brush_width = 1;
				request.tile_brush_height = 1;

				request.is_chroma_keyed = false;
				request.show_brush_previews = false;
				request.compression_algorithm = CompressionAlgorithm::LZSS;

				request.layout_type_name = "intro";
				request.layout_layout_name = "robotnik_ship";

				m_working_palette_set = *m_owning_ui.GetROM().GetIntroCutscenePaletteSet();

				m_tile_layout_render_requests.emplace_back(std::move(request));
			}
			break;

			case RenderRequestType::INTRO_WATER:
			{
				Reset();
				{
					RenderTileLayoutRequest request;

					request.tileset_address = 0x000A3124 + 2;

					// Water
					request.tile_layout_width = m_owning_ui.GetROM().ReadUint16(0x000a220c);
					request.tile_layout_height = m_owning_ui.GetROM().ReadUint16(0x000a220e);
					request.tile_layout_address = 0x000a220c;
					request.palette_line = 0;

					request.tile_brush_width = 1;
					request.tile_brush_height = 1;

					request.is_chroma_keyed = false;
					request.show_brush_previews = false;
					request.compression_algorithm = CompressionAlgorithm::LZSS;

					request.layout_type_name = "intro";
					request.layout_layout_name = "water";

					m_working_palette_set = *m_owning_ui.GetROM().GetIntroCutscenePaletteSet();

					m_tile_layout_render_requests.emplace_back(std::move(request));
				}
			}
			break;

			case RenderRequestType::MENU:
			{
				Reset();
				{
					RenderTileLayoutRequest request;

					request.tileset_address = 0x0009D102 + 2;
					request.tile_layout_width = m_owning_ui.GetROM().ReadUint16(0x0009C82E);
					request.tile_layout_height = m_owning_ui.GetROM().ReadUint16(0x0009C830);
					request.palette_line = 1;

					request.tile_layout_address = 0x0009C82E;

					request.tile_brush_width = 1;
					request.tile_brush_height = 1;

					request.is_chroma_keyed = false;
					request.show_brush_previews = false;
					request.compression_algorithm = CompressionAlgorithm::LZSS;

					request.layout_type_name = "intro";
					request.layout_layout_name = "bg";

					m_working_palette_set = *m_owning_ui.GetROM().GetMainMenuPaletteSet();
					m_tile_layout_render_requests.emplace_back(std::move(request));
				}

				{
					RenderTileLayoutRequest request;

					request.tileset_address = 0x0009D102 + 2;
					request.tile_layout_width = m_owning_ui.GetROM().ReadUint16(0x0009C05A);
					request.tile_layout_height = m_owning_ui.GetROM().ReadUint16(0x0009C05C);
					request.tile_layout_address = 0x0009C05A;
					request.palette_line = 0;

					request.tile_brush_width = 1;
					request.tile_brush_height = 1;

					request.is_chroma_keyed = true;
					request.show_brush_previews = false;
					request.compression_algorithm = CompressionAlgorithm::LZSS;

					request.layout_type_name = "intro";
					request.layout_layout_name = "fg";

					m_working_palette_set = *m_owning_ui.GetROM().GetMainMenuPaletteSet();
					m_tile_layout_render_requests.emplace_back(std::move(request));
				}
			}
			break;

			case RenderRequestType::BONUS:
			{
				Reset();
				{
					RenderTileLayoutRequest request;

					request.tileset_address = 0x000C77B0;
					request.tile_layout_address = 0x000C7350 - 4;
					request.tile_layout_address_end = 0x000c77b0 - 4;
					request.palette_line = m_preview_bonus_alt_palette ? 1 : 0;

					request.tile_brush_width = 1;
					request.tile_brush_height = 1;

					request.tile_layout_width = 0x14;
					request.tile_layout_height = static_cast<unsigned int>(((*request.tile_layout_address_end - request.tile_layout_address) / 2) / request.tile_layout_width);

					request.is_chroma_keyed = false;
					request.show_brush_previews = false;
					request.draw_mirrored_layout = true;
					request.compression_algorithm = CompressionAlgorithm::LZSS;

					m_working_palette_set = rom::PaletteSet{};
					m_working_palette_set.palette_lines[0] = m_owning_ui.GetPalettes()[0x1f];
					m_working_palette_set.palette_lines[1] = m_owning_ui.GetPalettes()[0x20];
					m_working_palette_set.palette_lines[2] = m_owning_ui.GetPalettes()[0x21];
					m_working_palette_set.palette_lines[3] = m_owning_ui.GetPalettes()[0x22];

					request.layout_type_name = "bonus";
					request.layout_layout_name = "bg";

					m_tile_layout_render_requests.emplace_back(std::move(request));
				}

				{
					RenderTileLayoutRequest request;

					request.tileset_address = 0x000C77B0;
					request.tile_layout_address = 0x000C6EF0 - 4;
					request.tile_layout_address_end = 0x000C734D;
					request.palette_line = m_preview_bonus_alt_palette ? 0 : 1;

					request.tile_brush_width = 1;
					request.tile_brush_height = 1;

					request.tile_layout_width = 0x14;
					request.tile_layout_height = static_cast<unsigned int>(((*request.tile_layout_address_end - request.tile_layout_address) / 2) / request.tile_layout_width);

					request.is_chroma_keyed = true;
					request.show_brush_previews = false;
					request.draw_mirrored_layout = true;
					request.compression_algorithm = CompressionAlgorithm::LZSS;

					m_working_palette_set = rom::PaletteSet{};
					m_working_palette_set.palette_lines[0] = m_owning_ui.GetPalettes()[0x1f];
					m_working_palette_set.palette_lines[1] = m_owning_ui.GetPalettes()[0x20];
					m_working_palette_set.palette_lines[2] = m_owning_ui.GetPalettes()[0x21];
					m_working_palette_set.palette_lines[3] = m_owning_ui.GetPalettes()[0x22];

					request.layout_type_name = "bonus";
					request.layout_layout_name = "fg";

					m_tile_layout_render_requests.emplace_back(std::move(request));
				}
			}
			break;

			case RenderRequestType::SEGA_LOGO:
			{
				Reset();
				{
					RenderTileLayoutRequest request;

					request.tileset_address = 0x000A3124 + 2;
					request.tile_layout_width = m_owning_ui.GetROM().ReadUint16(0x000A14F8);
					request.tile_layout_height = m_owning_ui.GetROM().ReadUint16(0x000A14FA);
					request.tile_layout_address = 0x000A14F8;

					request.tile_brush_width = 1;
					request.tile_brush_height = 1;

					request.is_chroma_keyed = false;
					request.show_brush_previews = false;
					request.compression_algorithm = CompressionAlgorithm::LZSS;

					request.layout_type_name = "intro";
					request.layout_layout_name = "sega_logo";

					m_working_palette_set = *m_owning_ui.GetROM().GetSegaLogoIntroPaletteSet();
					m_tile_layout_render_requests.emplace_back(std::move(request));
				}
			}
			break;

		}
	}

	bool EditorTileLayoutViewer::IsDraggingObject() const
	{
		if (m_working_game_obj)
		{
			return m_working_game_obj->destination != nullptr && ImGui::IsPopupOpen("obj_popup") == false;
		}

		if (m_working_spline)
		{
			return m_working_spline->dest_spline_point != nullptr;
		}

		return false;
	}

	bool EditorTileLayoutViewer::IsObjectPopupOpen() const
	{
		return ImGui::IsPopupOpen("obj_popup") || ImGui::IsPopupOpen("spline_popup");
	}

	bool EditorTileLayoutViewer::IsEditingSomething() const
	{
		return m_working_spline.has_value() || m_working_game_obj.has_value() || m_working_brush.has_value() || m_selected_tile.HasSelection() || m_selected_brush.HasSelection() || m_working_flipper.has_value() || m_working_ring.has_value() || IsObjectPopupOpen() || IsDraggingObject();
	}

	void EditorTileLayoutViewer::TestCollisionCullingResults() const
	{
		// Spline culling unit test
		SplineManager boogaloo;
		boogaloo.LoadFromSplineCullingTable(*m_spline_manager.GenerateSplineCullingTable());

		const auto rom_table = rom::SplineCullingTable::LoadFromROM(m_owning_ui.GetROM(), m_owning_ui.GetROM().ReadUint32(m_level->m_data_offsets.collision_data_terrain));
		const auto og_table = m_spline_manager.GenerateSplineCullingTable();
		const auto new_table = boogaloo.GenerateSplineCullingTable();

		for (size_t i = 0; i < rom_table.cells.size(); ++i)
		{
			const rom::SplineCullingCell& rom_cell = rom_table.cells[i];
			const rom::SplineCullingCell& og_cell = og_table->cells[i];
			const rom::SplineCullingCell& new_cell = og_table->cells[i];

			bool orders_mismatch = false;
			orders_mismatch = og_cell.splines.size() == new_cell.splines.size() && og_cell.splines.size() == rom_cell.splines.size();
			for (size_t c = 0; c < og_cell.splines.size(); ++c)
			{
				assert(og_cell.splines[c] == new_cell.splines[c]);
				if (og_cell.splines[c] == new_cell.splines[c] && og_cell.splines[c] == rom_cell.splines[c])
				{
					continue;
				}
				else
				{
					orders_mismatch = true;
					break;
				}

				if ((m_spline_manager.splines[c] == boogaloo.splines[c]) == false)
				{
					break;
				}
			}

			if (orders_mismatch)
			{
				for (const rom::CollisionSpline& spline : og_cell.splines)
				{
					assert(std::any_of(std::begin(rom_cell.splines), std::end(rom_cell.splines), [&spline](const rom::CollisionSpline& _spline)
						{
							return _spline == spline;
						}));
				}

				for (const rom::CollisionSpline& spline : rom_cell.splines)
				{
					assert(std::any_of(std::begin(og_cell.splines), std::end(og_cell.splines), [&spline](const rom::CollisionSpline& _spline)
						{
							return _spline == spline;// || spline.extra_info == 0x0006;
						}));

					//if (std::none_of(std::begin(og_cell.splines), std::end(og_cell.splines), [&spline](const rom::CollisionSpline& _spline)
					//	{
					//		return _spline == spline;// || spline.extra_info == 0x0006;
					//	}))
					//{
					//	// Did we lose this spline entirely, or was this some redunant data?
					//
					//	assert(std::any_of(std::begin(og_table.cells), std::end(og_table.cells),
					//		[&spline](const rom::SplineCullingCell& _cell)
					//		{
					//			return std::any_of(std::begin(_cell.splines), std::end(_cell.splines),
					//				[&spline](const rom::CollisionSpline& _spline)
					//				{
					//					return _spline == spline;
					//				});
					//		}));
					//}
				}
			}

			if ((og_cell.splines.size() == new_cell.splines.size() && og_cell.splines.size() == rom_cell.splines.size()) == false)
			{
				break;
			}

		}
	}

	void EditorTileLayoutViewer::Reset()
	{
		m_level.reset();
		m_spline_manager = {};
		m_game_object_manager = {};
	}

	void EditorTileLayoutViewer::DrawCollisionSpline(rom::CollisionSpline& spline, ImVec2 origin, ImVec2 screen_origin, LayerSettings& current_layer_settings, bool is_working_spline, bool draw_bbox)
	{
		const BoundingBox& spline_bbox = spline.spline_vector;
		ImVec4 colour{ 192,192,0,128 };
		if (spline.IsBumper())
		{
			colour = ImVec4(1.0f, 0, 1.0f, 1.0f);
		}
		else if (spline.IsTeleporter())
		{
			colour = ImVec4(0, 1.0f, 1.0f, 1.0f);
		}
		else if (spline.IsSlippery())
		{
			colour = ImVec4(0, 0.5f, 1.0f, 1.0f);
		}
		else if (spline.IsRing())
		{
			colour = ImVec4(1.0f, 0.5f, 0, 1.0f);
		}
		else if (spline.IsWalkable())
		{
			colour = ImVec4(0.5f, 0.5f, 1.0f, 1.0f);
		}
		else if (spline.IsUnknown())
		{
			colour = ImVec4(0.5f, 0.5f, 1.0f, 1.0f);
		}

		if (spline.IsRadial())
		{
			const BoundingBox& original_bbox = spline.spline_vector;
			BoundingBox fixed_bbox;
			fixed_bbox.min.x = spline.spline_vector.min.x - spline.spline_vector.max.x - 2;
			fixed_bbox.max.x = spline.spline_vector.min.x + spline.spline_vector.max.x + 2;
			fixed_bbox.min.y = spline.spline_vector.min.y - spline.spline_vector.max.x - 2;
			fixed_bbox.max.y = spline.spline_vector.min.y + spline.spline_vector.max.x + 2;

			if (ImGui::IsClippedEx((fixed_bbox * m_zoom) + screen_origin, 0) == true)
			{
				return;
			}
			ImGui::GetWindowDrawList()->AddCircle(screen_origin + (spline.spline_vector.min * m_zoom), static_cast<float>(spline.spline_vector.max.x) * m_zoom
				, ImGui::GetColorU32(colour), 16, spline.object_type_flags != 0x8000 ? 1.0f : 2.0f);

			ImGui::GetWindowDrawList()->AddRect(
				ImVec2{ static_cast<float>(screen_origin.x + (spline.spline_vector.min.x * m_zoom) - 2), static_cast<float>(screen_origin.y + (spline.spline_vector.min.y * m_zoom) - 2) },
				ImVec2{ static_cast<float>(screen_origin.x + (spline.spline_vector.min.x * m_zoom) + 3), static_cast<float>(screen_origin.y + (spline.spline_vector.min.y * m_zoom) + 3) },
				ImGui::GetColorU32(colour), 0, ImDrawFlags_None, 1.0f);

			if (current_layer_settings.spline_culling == true)
			{
				return;
			}

			if (is_working_spline || (m_working_spline.has_value() == false && ImGui::IsMouseHoveringRect(screen_origin + (fixed_bbox.min * m_zoom), screen_origin + (fixed_bbox.max * m_zoom))))
			{
				ImGui::GetWindowDrawList()->AddRect(screen_origin + (fixed_bbox.min * m_zoom), screen_origin + (fixed_bbox.max * m_zoom),
					ImGui::GetColorU32(colour), 0, ImDrawFlags_None, 3.0f);

				if (m_working_spline.has_value() == false && current_layer_settings.hover_radials && ImGui::BeginTooltip())
				{
					ImGui::SeparatorText("Radial Collision");
					ImGui::Text("Position: X: 0x%04X  Y: 0x%04X", spline.spline_vector.min.x, spline.spline_vector.min.y);
					ImGui::Text("Radius: 0x%04X", spline.spline_vector.max.x);
					ImGui::Text("Unused Y component: 0x%04X", spline.spline_vector.max.y);
					ImGui::Text("Obj Type Flags: 0x%04X", spline.object_type_flags);
					ImGui::Text("Instance ID Binding: 0x%04X", spline.instance_id_binding);

					ImGui::EndTooltip();
				}

				if (m_working_spline.has_value() == false && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
				{
					//if (spline.IsRing() == false && spline.IsTeleporter())
					{
						WorkingSpline new_spline;
						new_spline.destination = &spline;
						new_spline.spline = spline;
						m_working_spline.emplace(new_spline);
						m_working_spline->current_mode = WorkingSplineMode::DRAGGING_POINT;
						m_working_spline->dest_spline_point = &m_working_spline->spline.spline_vector.min;
					}
				}

				if (m_working_spline.has_value() == false && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
				{
					ImGui::OpenPopup("spline_popup");
					WorkingSpline new_spline;
					new_spline.destination = &spline;
					new_spline.spline = spline;
					m_working_spline.emplace(new_spline);
					m_working_spline->current_mode = WorkingSplineMode::EDITING_SPLINE_PROPERTIES;
				}
			}
		}
		else
		{
			const BoundingBox& original_bbox = spline.spline_vector;
			BoundingBox fixed_bbox;
			fixed_bbox.min.x = std::min(spline.spline_vector.min.x, spline.spline_vector.max.x) - 1;
			fixed_bbox.max.x = std::max(spline.spline_vector.min.x, spline.spline_vector.max.x) + 1;
			fixed_bbox.min.y = std::min(spline.spline_vector.min.y, spline.spline_vector.max.y) - 1;
			fixed_bbox.max.y = std::max(spline.spline_vector.min.y, spline.spline_vector.max.y) + 1;

			if (ImGui::IsClippedEx((fixed_bbox * m_zoom) + screen_origin, 0) == true)
			{
				return;
			}

			ImGui::GetWindowDrawList()->AddLine(screen_origin + (spline.spline_vector.min * m_zoom), screen_origin + (spline.spline_vector.max * m_zoom)
				, ImGui::GetColorU32(colour), 2.0f);
			ImGui::SetCursorPos(origin + (spline.spline_vector.min * m_zoom));

			if (current_layer_settings.spline_culling == true)
			{
				return;
			}

			if (draw_bbox)
			{
				ImU32 bbox_colour = ImGui::GetColorU32(ImVec4{ 1.0f,1.0f,0.0f,1.0f });
				ImGui::GetForegroundDrawList()->AddRect((screen_origin + (fixed_bbox.min * m_zoom)), (screen_origin + (fixed_bbox.max * m_zoom)), bbox_colour, 0, ImDrawFlags_None, 2.0f);
			}

			if (is_working_spline || (m_working_spline.has_value() == false && ImGui::IsMouseHoveringRect(screen_origin + (fixed_bbox.min * m_zoom), screen_origin + (fixed_bbox.max * m_zoom))))
			{
				constexpr float handle_size = 4.0f;
				constexpr ImVec2 handle_dimensions{ 4.0f, 4.0f };

				ImU32 start_handle_colour = ImGui::GetColorU32(ImVec4{ 1.0f,0.0f,1.0f,1.0f });
				ImU32 end_handle_colour = ImGui::GetColorU32(ImVec4{ 1.0f,0.0f,1.0f,1.0f });

				const ImVec2 spline_vector{ (original_bbox.max - original_bbox.min) * m_zoom };
				const float spline_length = std::sqrtf(std::powf(spline_vector.x, 2) + std::powf(spline_vector.y, 2));

				const ImVec2 spline_to_cursor_vector{ ImGui::GetMousePos() - (screen_origin + (original_bbox.min * m_zoom)) };
				const float spline_to_cursor_length = std::sqrtf(std::powf(spline_to_cursor_vector.x, 2) + std::powf(spline_to_cursor_vector.y, 2));

				if (is_working_spline || spline_to_cursor_length <= spline_length)
				{
					const float distance_delta = spline_to_cursor_length / spline_length;
					const ImVec2 collision_test_pos{ (original_bbox.min * m_zoom) + (spline_vector * distance_delta) };
					bool is_hovering_handles = false;

					if (ImGui::IsMouseHoveringRect((screen_origin + (original_bbox.min * m_zoom)) - handle_dimensions, (screen_origin + (original_bbox.min * m_zoom)) + handle_dimensions))
					{
						is_hovering_handles = true;
						start_handle_colour = ImGui::GetColorU32(ImVec4{ 128,255,128,255 });
						if (m_working_spline.has_value() == false && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
						{
							WorkingSpline new_spline;
							new_spline.destination = &spline;
							new_spline.spline = spline;
							m_working_spline.emplace(new_spline);
							m_working_spline->current_mode = WorkingSplineMode::DRAGGING_POINT;
							m_working_spline->dest_spline_point = &m_working_spline->spline.spline_vector.min;
						}
					}
					else if (ImGui::IsMouseHoveringRect((screen_origin + (original_bbox.max * m_zoom)) - handle_dimensions, (screen_origin + (original_bbox.max * m_zoom)) + handle_dimensions))
					{
						is_hovering_handles = true;
						end_handle_colour = ImGui::GetColorU32(ImVec4{ 128,255,128,255 });
						if (m_working_spline.has_value() == false && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
						{
							WorkingSpline new_spline;
							new_spline.destination = &spline;
							new_spline.spline = spline;
							m_working_spline.emplace(new_spline);
							m_working_spline->current_mode = WorkingSplineMode::DRAGGING_POINT;
							m_working_spline->dest_spline_point = &m_working_spline->spline.spline_vector.max;
						}
					}

					ImGui::GetForegroundDrawList()->AddRect((screen_origin + (original_bbox.min * m_zoom)) - handle_dimensions, (screen_origin + (original_bbox.min * m_zoom)) + handle_dimensions,
						start_handle_colour, 0, ImDrawFlags_None, 2.0f);

					ImGui::GetForegroundDrawList()->AddRect((screen_origin + (original_bbox.max * m_zoom)) - handle_dimensions, (screen_origin + (original_bbox.max * m_zoom)) + handle_dimensions,
						end_handle_colour, 0, ImDrawFlags_None, 2.0f);

					const bool is_able_to_click = is_hovering_handles == false && (ImGui::IsMouseHoveringRect(screen_origin + collision_test_pos - handle_dimensions, screen_origin + collision_test_pos + handle_dimensions));

					if (is_able_to_click)
					{
						ImU32 target_handle_colour = ImGui::GetColorU32(ImVec4{ 255,0,0,255 });

						ImGui::GetForegroundDrawList()->AddRect((screen_origin + collision_test_pos) - handle_dimensions, (screen_origin + collision_test_pos) + handle_dimensions,
							ImGui::GetColorU32(ImVec4{ 255,0,0,255 }), 0, ImDrawFlags_None, 2.0f);

						if (m_working_spline.has_value() == false && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
						{
							ImGui::OpenPopup("spline_popup");
							WorkingSpline new_spline;
							new_spline.destination = &spline;
							new_spline.spline = spline;
							m_working_spline.emplace(new_spline);
							m_working_spline->current_mode = WorkingSplineMode::EDITING_SPLINE_PROPERTIES;
						}
					}
				}
			}
		}
	}
}