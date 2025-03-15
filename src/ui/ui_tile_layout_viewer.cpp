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

		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				ImGui::BeginDisabled(m_level == nullptr || m_level->level_index == -1);

				if (ImGui::Selectable("Repaint Level"))
				{
					m_render_from_edit = true;
					out_render_request = RenderRequestType::LEVEL;
				}

				if (ImGui::Selectable("Save Level"))
				{
					if (m_level && m_level->m_tile_layers.size() == 2)
					{
						//for (TileLayer& layer : m_level->m_tile_layers)
						{
							m_level->m_tile_layers[0].tile_layout->SaveToROM(m_owning_ui.GetROM(), m_owning_ui.GetROM().ReadUint32(m_level_data_offsets.background_tile_layout));
							m_level->m_tile_layers[1].tile_layout->SaveToROM(m_owning_ui.GetROM(), m_owning_ui.GetROM().ReadUint32(m_level_data_offsets.foreground_tile_layout));
						}
					}
					m_spline_manager.GenerateSplineCullingTable().SaveToROM(m_owning_ui.GetROM(), m_owning_ui.GetROM().ReadUint32(m_level_data_offsets.collision_data_terrain));
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
					const int previous_level_index = m_level != nullptr ? m_level->level_index : -1;
					int level_index = -1;
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

					if (level_index != -1 && level_index != previous_level_index)
					{
						m_level = std::make_shared<Level>();
						m_level->level_index = level_index;
						m_level->m_tile_layers.emplace_back();
						m_level->m_tile_layers.emplace_back();
						m_level_data_offsets = rom::LevelDataOffsets{ m_level->level_index };
					}
					else if (level_index != -1)
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
				ImGui::Checkbox("Collision Culling Sectors", &m_layer_settings.collision_culling);
				ImGui::Checkbox("Visibility Culling Sectors", &m_layer_settings.visibility_culling);
				ImGui::SeparatorText("Tooltips");
				ImGui::Checkbox("Game Object Info", &m_layer_settings.hover_game_objects);
				ImGui::Checkbox("Spline Collision Info", &m_layer_settings.hover_splines);
				ImGui::Checkbox("Radial Collision Info", &m_layer_settings.hover_radials);
				ImGui::Checkbox("Tile Info", &m_layer_settings.hover_tiles);
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

		return out_render_request;
	}

	void EditorTileLayoutViewer::Update()
	{
		if (IsOpen() == false)
		{
			return;
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
			
			if (render_request == RenderRequestType::LEVEL)
			{
				render_flippers = true;
				render_rings = true;
			}

			if (render_flippers)
			{
				const static Uint32 flippers_ptr_table_offset = 0x000C08BE;
				const static Uint32 flippers_count_table_offset = 0x000C08EE;
				const static Uint32 flipper_palette[4] =
				{
					0x1,
					0x0,
					0x0,
					0x0
				};
				const static Uint32 flipper_sprite_offset[4] =
				{
					0x00017E82,
					0x0002594A,
					0x0002E05C,
					0x00035692
				};

				if (m_flipper_preview.sprite == nullptr || m_render_from_edit == false)
				{
					std::shared_ptr<const rom::Sprite> flipperSprite = rom::Sprite::LoadFromROM(m_owning_ui.GetROM(), flipper_sprite_offset[m_level->level_index]);
					m_flipper_preview.sprite = SDLSurfaceHandle{ SDL_CreateSurface(44, 31, SDL_PIXELFORMAT_INDEX8) };
					m_flipper_preview.palette = Renderer::CreateSDLPalette(*m_working_palette_set.palette_lines[flipper_palette[m_level->level_index]]);
					SDL_SetSurfacePalette(m_flipper_preview.sprite.get(), m_flipper_preview.palette.get());
					SDL_ClearSurface(m_flipper_preview.sprite.get(), 0.0f, 0.0f, 0.0f, 0.0f);
					SDL_SetSurfaceColorKey(m_flipper_preview.sprite.get(), true, SDL_MapRGBA(SDL_GetPixelFormatDetails(m_flipper_preview.sprite->format), nullptr, 0, static_cast<Uint8>(rom::Swatch::Pack(0, rom::Colour::levels_lookup[0x0], rom::Colour::levels_lookup[0x0])), 0, 0));
					flipperSprite->RenderToSurface(m_flipper_preview.sprite.get());
				}
				const Uint32 flippers_table_begin = m_owning_ui.GetROM().ReadUint32(flippers_ptr_table_offset + (4 * m_level->level_index));
				const Uint32 num_flippers = m_owning_ui.GetROM().ReadUint16(flippers_count_table_offset + (2 * m_level->level_index));

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
					SDL_ClearSurface(m_ring_preview.sprite.get(), 0.0f, 0.0f, 0.0f, 0.0f);
					SDL_SetSurfaceColorKey(m_ring_preview.sprite.get(), true, SDL_MapRGBA(SDL_GetPixelFormatDetails(m_ring_preview.sprite->format), nullptr, 0, 0, 0, 0));
					ring_sprite->RenderToSurface(m_ring_preview.sprite.get());
				}

				m_level->m_ring_instances.clear();
				m_level->m_ring_instances.reserve(m_level_data_offsets.ring_instances.count);

				{
					Uint32 current_table_offset = m_level_data_offsets.ring_instances.offset;

					for (Uint32 i = 0; i < m_level_data_offsets.ring_instances.count; ++i)
					{
						m_level->m_ring_instances.emplace_back(rom::RingInstance::LoadFromROM(m_owning_ui.GetROM(), current_table_offset));
						current_table_offset = m_level->m_ring_instances.back().rom_data.rom_offset_end;
					}
				}
				/////////////

				const static size_t game_obj_sprite_offset = 0x0000F6D8;

				std::shared_ptr<const rom::Sprite> LevelGameObjSprite = rom::Sprite::LoadFromROM(m_owning_ui.GetROM(), game_obj_sprite_offset);
				m_game_object_preview.sprite = SDLSurfaceHandle{ SDL_CreateSurface(LevelGameObjSprite->GetBoundingBox().Width(), LevelGameObjSprite->GetBoundingBox().Height(), SDL_PIXELFORMAT_RGBA32) };
				m_game_object_preview.palette = Renderer::CreateSDLPalette(*m_working_palette_set.palette_lines[3].get());
				SDL_SetSurfacePalette(m_game_object_preview.sprite.get(), m_game_object_preview.palette.get());
				SDL_ClearSurface(m_game_object_preview.sprite.get(), 255, 255, 255, 255);
				SDL_SetSurfaceColorKey(m_game_object_preview.sprite.get(), true, SDL_MapRGBA(SDL_GetPixelFormatDetails(m_game_object_preview.sprite->format), nullptr, 255, 0, 0, 255));
				//LevelGameObjSprite->RenderToSurface(GameObjectPreview.sprite.get());

				m_spline_manager.LoadFromSplineCullingTable(rom::SplineCullingTable::LoadFromROM(m_owning_ui.GetROM(), m_owning_ui.GetROM().ReadUint32(m_level_data_offsets.collision_data_terrain)));

				m_preview_game_objects.clear();
				m_anim_sprite_instances.clear();
				m_level->m_game_obj_instances.clear();
				m_level->m_game_obj_instances.reserve(m_level_data_offsets.object_instances.count);

				{
					Uint32 current_table_offset = m_level_data_offsets.object_instances.offset;

					for (Uint32 i = 0; i < m_level_data_offsets.object_instances.count; ++i)
					{
						m_level->m_game_obj_instances.emplace_back(rom::GameObjectDefinition::LoadFromROM(m_owning_ui.GetROM(), current_table_offset));
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
								SDL_ClearSurface(new_sprite_surface.get(), 0.0f, 0.0f, 0.0f, 0.0f);
								SDL_SetSurfaceColorKey(new_sprite_surface.get(), true, SDL_MapRGBA(SDL_GetPixelFormatDetails(new_sprite_surface->format), nullptr, 0, 0, 0, 0));
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
				
				if (request.compression_algorithm == CompressionAlgorithm::SSC)
				{
					if (request.store_tileset != nullptr && *request.store_tileset != nullptr)
					{
						m_working_tileset = *request.store_tileset;
						preserve_rendered_items = true;
					}
					else
					{
						m_working_tileset = rom::TileSet::LoadFromROM(m_owning_ui.GetROM(), request.tileset_address).tileset;
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
						m_working_tile_layout = rom::TileLayout::LoadFromROM(m_owning_ui.GetROM(), request.tile_brushes_address, request.tile_brushes_address_end, request.tile_layout_address, request.tile_layout_address_end);
						if (request.store_layout != nullptr)
						{
							*request.store_layout = m_working_tile_layout;
						}
					}
				}
				else if (request.compression_algorithm == CompressionAlgorithm::LZSS)
				{
					if (request.store_tileset != nullptr && *request.store_tileset != nullptr)
					{
						m_working_tileset = *request.store_tileset;
						preserve_rendered_items = true;
					}
					else
					{
						m_working_tileset = rom::TileSet::LoadFromROM_LZSSCompression(m_owning_ui.GetROM(), request.tileset_address).tileset;
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
						m_working_tile_layout = rom::TileLayout::LoadFromROM(m_owning_ui.GetROM(), *m_working_tileset, request.tile_layout_address, request.tile_layout_address_end);
						if (request.store_layout != nullptr)
						{
							*request.store_layout = m_working_tile_layout;
						}
					}
				}

				m_working_tile_layout->layout_width = request.tile_layout_width;
				m_working_tile_layout->layout_height = request.tile_layout_height;

				if (export_combined == false)
				{
					current_preview_data->tile_layout_address = m_working_tile_layout->rom_data.rom_offset;
					current_preview_data->tile_layout_address_end = m_working_tile_layout->rom_data.rom_offset_end;
				}
				std::shared_ptr<const rom::Sprite> tileset_sprite = m_working_tileset->CreateSpriteFromTile(0);
				std::shared_ptr<const rom::Sprite> tile_layout_sprite = std::make_unique<rom::Sprite>();


				const int brush_width = static_cast<int>(request.tile_brush_width * rom::TileSet::s_tile_width);
				const int brush_height = static_cast<int>(request.tile_brush_height * rom::TileSet::s_tile_height);

				if (preserve_rendered_items == false)
				{
					std::vector<rom::Sprite> brushes;
					m_tileset_preview_list.emplace_back();
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
						SDL_SetSurfaceColorKey(new_surface.get(), request.is_chroma_keyed, SDL_MapRGBA(SDL_GetPixelFormatDetails(new_surface->format), nullptr, 0, 0, 0, 0));
						SDL_ClearSurface(new_surface.get(), 0.0f, 0, 0, 0);
						brush_sprite.RenderToSurface(new_surface.get());
						SDL_Surface* the_surface = new_surface.get();
						brush_previews.emplace_back(TileBrushPreview{ std::move(new_surface), Renderer::RenderToTexture(the_surface), static_cast<Uint32>(brush_index) });
					}
				}

				const size_t target_index = m_level == nullptr || m_working_tile_layout == m_level->m_tile_layers[0].tile_layout ? 0 : 1;
				auto& brush_previews = m_tileset_preview_list.at(target_index).brushes;

				for (size_t i = 0; i < m_working_tile_layout->tile_brush_instances.size(); ++i)
				{
					const auto tile_brush_index = m_working_tile_layout->tile_brush_instances[i].tile_brush_index;
					if (tile_brush_index >= brush_previews.size())
					{
						break;
					}

					SDLSurfaceHandle temp_surface{ SDL_DuplicateSurface(brush_previews[tile_brush_index].surface.get()) };
					if (m_working_tile_layout->tile_brush_instances[i].is_flipped_horizontally)
					{
						SDL_FlipSurface(temp_surface.get(), SDL_FLIP_HORIZONTAL);
					}

					if (m_working_tile_layout->tile_brush_instances[i].is_flipped_vertically)
					{
						SDL_FlipSurface(temp_surface.get(), SDL_FLIP_VERTICAL);
					}

					const int x_off = static_cast<int>((i % request.tile_layout_width) * brush_width);
					const int y_off = static_cast<int>(((i - (i % request.tile_layout_width)) / request.tile_layout_width) * brush_height);

					const SDL_Rect target_rect{ x_off, y_off, brush_width, brush_height };
					SDL_SetSurfaceColorKey(temp_surface.get(), request.is_chroma_keyed, SDL_MapRGBA(SDL_GetPixelFormatDetails(temp_surface->format), nullptr, 0, 0, 0, 0));
					SDL_BlitSurface(temp_surface.get(), nullptr, layout_preview_bg_surface.get(), &target_rect);
				}

				if (request.draw_mirrored_layout)
				{
					for (size_t i = 0; i < m_working_tile_layout->tile_brush_instances.size(); ++i)
					{
						const auto tile_brush_index = m_working_tile_layout->tile_brush_instances[i].tile_brush_index;
						if (tile_brush_index >= brush_previews.size())
						{
							break;
						}

						SDLSurfaceHandle temp_surface{ SDL_DuplicateSurface(brush_previews[tile_brush_index].surface.get()) };
						if (m_working_tile_layout->tile_brush_instances[i].is_flipped_horizontally == false)
						{
							SDL_FlipSurface(temp_surface.get(), SDL_FLIP_HORIZONTAL);
						}

						if (m_working_tile_layout->tile_brush_instances[i].is_flipped_vertically)
						{
							SDL_FlipSurface(temp_surface.get(), SDL_FLIP_VERTICAL);
						}

						const int x_off = ((request.tile_layout_width * 2 * brush_width)- brush_width) - static_cast<int>((i % request.tile_layout_width) * brush_width);
						const int y_off = static_cast<int>(((i - (i % request.tile_layout_width)) / request.tile_layout_width) * brush_height);

						const SDL_Rect target_rect{ x_off, y_off, brush_width, brush_height };
						SDL_SetSurfaceColorKey(temp_surface.get(), request.is_chroma_keyed, SDL_MapRGBA(SDL_GetPixelFormatDetails(temp_surface->format), nullptr, 0, 0, 0, 0));
						SDL_BlitSurface(temp_surface.get(), nullptr, layout_preview_bg_surface.get(), &target_rect);
					}
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

					SDL_Rect target_rect{ flipper.x_pos + x_off, flipper.y_pos - 31, 44, 31 };
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
					SDL_Rect target_rect{ ring.x_pos - 8, ring.y_pos - 16, 0x10, 0x10 };
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

						SDL_SetSurfaceColorKey(temp_sprite_surface.get(), true, SDL_MapRGBA(SDL_GetPixelFormatDetails(temp_sprite_surface->format), nullptr, 0, 0, 0, 255));
						SDL_BlitSurfaceScaled(temp_sprite_surface.get(), nullptr, layout_preview_bg_surface.get(), &sprite_target_rect, SDL_SCALEMODE_NEAREST);

						std::unique_ptr<UIGameObject> new_obj = std::make_unique<UIGameObject>();
						new_obj->obj_definition = game_obj;
						new_obj->pos = ImVec2{ static_cast<float>(sprite_target_rect.x), static_cast<float>(sprite_target_rect.y) };
						new_obj->dimensions = ImVec2{ static_cast<float>(sprite_target_rect.w) , static_cast<float>(sprite_target_rect.h) };
						new_obj->ui_sprite = command_sprite_came_from.ui_frame_sprite;
						new_obj->sprite_table_address = animSpriteEntry.sprite_table;
						m_preview_game_objects.emplace_back(std::move(new_obj));
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

			constexpr const float zoom = 1.0f;
			bool has_just_selected_brush = false;
			static int selected_collision_tile_index = -1;

			ImGui::BeginGroup();
			{
				if (ImGui::BeginChild("InfoSizebar", ImVec2{ 340, -1 }, 0, ImGuiWindowFlags_AlwaysVerticalScrollbar))
				{
					//ImGui::SliderInt("Collision Tile", &selected_collision_tile_index, -1, 0x0FF - 1);

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
						if (m_tileset_preview_list.empty() == false && ImGui::BeginTabItem("Brush Previews"))
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
								for (size_t layer_index = 0; layer_index < m_tileset_preview_list.size(); ++layer_index)
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
										ImGui::SameLine();
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

												ImGui::Image((ImTextureID)preview_brush.texture.get(), ImVec2(static_cast<float>(preview_brush.texture->w) * zoom, static_cast<float>(preview_brush.texture->h) * zoom));
												if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
												{
													m_selected_brush.tileset = &tileset_preview;
													m_selected_brush.tile_brush = &preview_brush;
													m_selected_brush.tile_layer = m_level ? &m_level->m_tile_layers[layer_index] : nullptr;
													has_just_selected_brush = true;
												}
												if (ImGui::BeginItemTooltip())
												{
													ImGui::Text("Tile Index: 0x%02X", page_index);
													ImGui::EndTooltip();
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
					
						if (ImGui::BeginTabItem("Palettes"))
						{
							if (m_level != nullptr)
							{
								for (const TileLayer& tile_layer : m_level->m_tile_layers)
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
					if (m_tile_layout_preview_bg != nullptr || m_tile_layout_preview_fg != nullptr)
					{
						LayerSettings current_layer_settings = m_layer_settings;

						const ImVec2 origin = ImGui::GetCursorPos();
						const ImVec2 screen_origin = ImGui::GetCursorScreenPos();
						const ImVec2 mouse_pos = ImGui::GetMousePos();
						const ImVec2 relative_mouse_pos{ (mouse_pos.x - panel_screen_origin.x) + origin.x, (mouse_pos.y - panel_screen_origin.y) + origin.x };
						const ImVec2 brush_dimensions{ (rom::TileBrush<4, 4>::s_brush_width * 8), (rom::TileBrush<4,4>::s_brush_height * 8) };
						const Point grid_pos{ static_cast<int>(relative_mouse_pos.x / brush_dimensions.x), static_cast<int>(relative_mouse_pos.y / brush_dimensions.y) };
						const ImVec2 snapped_pos{ static_cast<float>(grid_pos.x) * brush_dimensions.x, static_cast<float>(grid_pos.y) * brush_dimensions.y };
						const ImVec2 final_snapped_pos{ snapped_pos.x + screen_origin.x + (panel_screen_origin.x - screen_origin.x), snapped_pos.y + screen_origin.y + (panel_screen_origin.y - screen_origin.y) };
						
						ImGui::Image((ImTextureID)m_tile_layout_preview_bg.get(), ImVec2(static_cast<float>(m_tile_layout_preview_bg->w) * zoom, static_cast<float>(m_tile_layout_preview_bg->h) * zoom));
						ImGui::SetCursorPos(origin);
						ImGui::Image((ImTextureID)m_tile_layout_preview_fg.get(), ImVec2(static_cast<float>(m_tile_layout_preview_fg->w) * zoom, static_cast<float>(m_tile_layout_preview_fg->h) * zoom));

						// Visualise collision vectors
						constexpr int tile_width = 128;
						constexpr int num_tiles_x = 16;
						constexpr int size_of_preview_collision_boxes = 4;
						constexpr int half_size_of_preview_collision_boxes = size_of_preview_collision_boxes / 2;

						// Terrain Collision

						if (m_selected_brush.HasBrushSelected() || m_selected_brush.IsPickingBrushFromLayout())
						{
							current_layer_settings.collision = false;
							current_layer_settings.rings = false;
							current_layer_settings.flippers = false;
							current_layer_settings.visible_objects = false;
							current_layer_settings.hover_game_objects = false;
							current_layer_settings.hover_splines = false;
							current_layer_settings.hover_radials = false;
							current_layer_settings.hover_tiles = m_selected_brush.IsPickingBrushFromLayout();
						}

						if (current_layer_settings.collision == true)
						{
							for (rom::CollisionSpline& next_spline : m_spline_manager.splines)
							{
								const bool is_working_spline = (m_working_spline.has_value() && m_working_spline->destination == &next_spline && (m_working_spline->dest_spline_point != nullptr || ImGui::IsPopupOpen("spline_popup")));
								rom::CollisionSpline& spline = is_working_spline ? m_working_spline->spline : next_spline;

								const BoundingBox& spline_bbox = spline.spline_vector;
								ImVec4 colour{ 192,192,0,128 };
								if (spline.IsRadial() && spline.object_type_flags != 0x8000)
								{
									colour = ImVec4(255, 0, 255, 255);
								}

								if (spline.IsTeleporter())
								{
									colour = ImVec4(0, 255, 255, 255);
								}

								if (spline.IsUnknown())
								{
									colour = ImVec4(128, 128, 255, 255);
								}

								if (spline.IsRadial())
								{
									const BoundingBox& original_bbox = spline.spline_vector;
									BoundingBox fixed_bbox;
									fixed_bbox.min.x = spline.spline_vector.min.x - spline.spline_vector.max.x - 2;
									fixed_bbox.max.x = spline.spline_vector.min.x + spline.spline_vector.max.x + 2;
									fixed_bbox.min.y = spline.spline_vector.min.y - spline.spline_vector.max.x - 2;
									fixed_bbox.max.y = spline.spline_vector.min.y + spline.spline_vector.max.x + 2;

									ImGui::GetWindowDrawList()->AddCircle(
										ImVec2{ static_cast<float>(screen_origin.x + spline.spline_vector.min.x), static_cast<float>(screen_origin.y + spline.spline_vector.min.y) },
										static_cast<float>(spline.spline_vector.max.x), ImGui::GetColorU32(colour), 16, spline.object_type_flags != 0x8000 ? 1.0f : 2.0f);

									ImGui::GetWindowDrawList()->AddRect(
										ImVec2{ static_cast<float>(screen_origin.x + spline.spline_vector.min.x - 2), static_cast<float>(screen_origin.y + spline.spline_vector.min.y - 2) },
										ImVec2{ static_cast<float>(screen_origin.x + spline.spline_vector.min.x + 3), static_cast<float>(screen_origin.y + spline.spline_vector.min.y + 3) },
										ImGui::GetColorU32(colour), 0, ImDrawFlags_None, 1.0f);

									if (is_working_spline || ImGui::IsMouseHoveringRect(
										ImVec2{ static_cast<float>(screen_origin.x + fixed_bbox.min.x), static_cast<float>(screen_origin.y + fixed_bbox.min.y) },
										ImVec2{ static_cast<float>(screen_origin.x + fixed_bbox.max.x), static_cast<float>(screen_origin.y + fixed_bbox.max.y) }))
									{
										ImGui::GetWindowDrawList()->AddRect(
											ImVec2{ static_cast<float>(screen_origin.x + fixed_bbox.min.x), static_cast<float>(screen_origin.y + fixed_bbox.min.y) },
											ImVec2{ static_cast<float>(screen_origin.x + fixed_bbox.max.x), static_cast<float>(screen_origin.y + fixed_bbox.max.y) },
											ImGui::GetColorU32(colour), 0, ImDrawFlags_None, 3.0f);

										if (m_working_spline.has_value() == false && current_layer_settings.hover_radials && ImGui::BeginTooltip())
										{
											ImGui::SeparatorText("Radial Collision");
											ImGui::Text("Position: X: 0x%04X  Y: 0x%04X", spline.spline_vector.min.x, spline.spline_vector.min.y);
											ImGui::Text("Radius: 0x%04X", spline.spline_vector.max.x);
											ImGui::Text("Unused Y component: 0x%04X", spline.spline_vector.max.y);
											ImGui::Text("Obj Type Flags: 0x%04X", spline.object_type_flags);
											ImGui::Text("Extra Info: 0x%04X", spline.extra_info);

											ImGui::EndTooltip();
										}

										if (m_working_spline.has_value() == false && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
										{
											WorkingSpline new_spline;
											new_spline.destination = &spline;
											new_spline.spline = spline;
											m_working_spline.emplace(new_spline);
											m_working_spline->dest_spline_point = &m_working_spline->spline.spline_vector.min;
										}

										if (m_working_spline.has_value() == false && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
										{
											ImGui::OpenPopup("spline_popup");
											WorkingSpline new_spline;
											new_spline.destination = &spline;
											new_spline.spline = spline;
											m_working_spline.emplace(new_spline);
										}
									}
								}
								else
								{
									ImGui::GetWindowDrawList()->AddLine(
										ImVec2{ static_cast<float>(screen_origin.x + spline.spline_vector.min.x), static_cast<float>(screen_origin.y + spline.spline_vector.min.y) },
										ImVec2{ static_cast<float>(screen_origin.x + spline.spline_vector.max.x), static_cast<float>(screen_origin.y + spline.spline_vector.max.y) },
										ImGui::GetColorU32(colour), 2.0f);

									ImGui::SetCursorPos(ImVec2{ origin.x + spline.spline_vector.min.x, origin.y + spline.spline_vector.min.y });
									ImGui::Dummy(ImVec2{ static_cast<float>(spline.spline_vector.Width()), static_cast<float>(spline.spline_vector.Height()) });

									const BoundingBox& original_bbox = spline.spline_vector;
									BoundingBox fixed_bbox;
									fixed_bbox.min.x = std::min(spline.spline_vector.min.x, spline.spline_vector.max.x) - 1;
									fixed_bbox.max.x = std::max(spline.spline_vector.min.x, spline.spline_vector.max.x) + 1;
									fixed_bbox.min.y = std::min(spline.spline_vector.min.y, spline.spline_vector.max.y) - 1;
									fixed_bbox.max.y = std::max(spline.spline_vector.min.y, spline.spline_vector.max.y) + 1;

									if (is_working_spline || ImGui::IsMouseHoveringRect(
										ImVec2{ static_cast<float>(screen_origin.x + fixed_bbox.min.x), static_cast<float>(screen_origin.y + fixed_bbox.min.y) },
										ImVec2{ static_cast<float>(screen_origin.x + fixed_bbox.max.x), static_cast<float>(screen_origin.y + fixed_bbox.max.y) }))
									{
										constexpr float handle_size = 4.0f;

										ImU32 start_handle_colour = ImGui::GetColorU32(ImVec4{ 255,0,255,255 });
										ImU32 end_handle_colour = ImGui::GetColorU32(ImVec4{ 255,0,255,255 });

										if (ImGui::IsMouseHoveringRect(ImVec2{ static_cast<float>(screen_origin.x + original_bbox.min.x - handle_size), static_cast<float>(screen_origin.y + original_bbox.min.y - handle_size) },
											ImVec2{ static_cast<float>(screen_origin.x + original_bbox.min.x + handle_size), static_cast<float>(screen_origin.y + original_bbox.min.y + handle_size) }))
										{
											start_handle_colour = ImGui::GetColorU32(ImVec4{ 128,255,128,255 });
											if (m_working_spline.has_value() == false && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
											{
												WorkingSpline new_spline;
												new_spline.destination = &spline;
												new_spline.spline = spline;
												m_working_spline.emplace(new_spline);
												m_working_spline->dest_spline_point = &m_working_spline->spline.spline_vector.min;
											}
										}
										else if (ImGui::IsMouseHoveringRect(ImVec2{ static_cast<float>(screen_origin.x + original_bbox.max.x - handle_size), static_cast<float>(screen_origin.y + original_bbox.max.y - handle_size) },
											ImVec2{ static_cast<float>(screen_origin.x + original_bbox.max.x + handle_size), static_cast<float>(screen_origin.y + original_bbox.max.y + handle_size) }))
										{
											end_handle_colour = ImGui::GetColorU32(ImVec4{ 128,255,128,255 });
											if (m_working_spline.has_value() == false && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
											{
												WorkingSpline new_spline;
												new_spline.destination = &spline;
												new_spline.spline = spline;
												m_working_spline.emplace(new_spline);
												m_working_spline->dest_spline_point = &m_working_spline->spline.spline_vector.max;
											}
										}

										ImGui::GetForegroundDrawList()->AddRect(
											ImVec2{ static_cast<float>(screen_origin.x + original_bbox.min.x - handle_size), static_cast<float>(screen_origin.y + original_bbox.min.y - handle_size) },
											ImVec2{ static_cast<float>(screen_origin.x + original_bbox.min.x + handle_size), static_cast<float>(screen_origin.y + original_bbox.min.y + handle_size) },
											start_handle_colour, 0, ImDrawFlags_None, 2.0f);

										ImGui::GetForegroundDrawList()->AddRect(
											ImVec2{ static_cast<float>(screen_origin.x + original_bbox.max.x - handle_size), static_cast<float>(screen_origin.y + original_bbox.max.y - handle_size) },
											ImVec2{ static_cast<float>(screen_origin.x + original_bbox.max.x + handle_size), static_cast<float>(screen_origin.y + original_bbox.max.y + handle_size) },
											end_handle_colour, 0, ImDrawFlags_None, 2.0f);

										if (m_working_spline.has_value() == false && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
										{
											ImGui::OpenPopup("spline_popup");
											WorkingSpline new_spline;
											new_spline.destination = &spline;
											new_spline.spline = spline;
											m_working_spline.emplace(new_spline);
										}
									}
								}
							}
						}

						if (m_level && has_just_selected_brush == false)
						{
							const int grid_ref = (grid_pos.y * m_level->m_tile_layers[0].tile_layout->layout_width) + grid_pos.x;

							if (/*current_layer_settings.hover_tiles &&*/m_selected_brush.tile_layer && grid_ref >= 0 && grid_ref < m_selected_brush.tile_layer->tile_layout->tile_brush_instances.size())
							{
								const int hovered_index = m_selected_brush.tile_layer->tile_layout->tile_brush_instances.at(grid_ref).tile_brush_index;

								if (m_selected_brush.tileset != nullptr && hovered_index > 0 && hovered_index < m_selected_brush.tileset->brushes.size() && ImGui::BeginTooltip())
								{
									ImGui::Image((ImTextureID)m_selected_brush.tileset->brushes.at(hovered_index).texture.get(), ImVec2{ 64,64 });
									ImGui::EndTooltip();
								}
							}

							if (m_selected_brush.IsActive())
							{

								if (m_selected_brush.HasBrushSelected())
								{
									ImGui::SetCursorScreenPos(final_snapped_pos);
									ImVec2 uv0 = { 0,0 };
									ImVec2 uv1 = { 1,1 };
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
									ImGui::Image((ImTextureID)m_selected_brush.tile_brush->texture.get(), ImVec2{ static_cast<float>(m_selected_brush.tile_brush->texture->w), static_cast<float>(m_selected_brush.tile_brush->texture->h) }, uv0, uv1);
								}

								ImGui::GetForegroundDrawList()->AddRect(
									ImVec2{ final_snapped_pos.x - 1, final_snapped_pos.y - 1 },
									ImVec2{ final_snapped_pos.x + brush_dimensions.x + 1, final_snapped_pos.y + brush_dimensions.y + 1 },
									ImGui::GetColorU32(ImVec4{ 255,0,255,255 }), 0, 0, 1.0f);

								if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && m_selected_brush.tile_layer != nullptr)
								{
									if (m_selected_brush.IsPickingBrushFromLayout())
									{
										if (m_selected_brush.tile_layer->tile_layout->tile_brush_instances.empty() == false)
										{
											const int selected_index = m_selected_brush.tile_layer->tile_layout->tile_brush_instances.at(grid_ref).tile_brush_index;
											m_selected_brush.tile_brush = &m_selected_brush.tileset->brushes.at(selected_index);
											m_selected_brush.is_picking_from_layout = false;
											m_selected_brush.was_picked_from_layout = true;
										}
										else
										{
											m_selected_brush.Clear();
										}
										has_just_selected_brush = true;
									}
									else if (m_selected_brush.HasBrushSelected())
									{
										// Background
										if (grid_ref < m_selected_brush.tile_layer->tile_layout->tile_brush_instances.size())
										{
											auto& tile = m_selected_brush.tile_layer->tile_layout->tile_brush_instances.at(grid_ref);
											tile.tile_brush_index = m_selected_brush.tile_brush->brush_index;
											tile.is_flipped_horizontally = m_selected_brush.flip_x;
											tile.is_flipped_vertically = m_selected_brush.flip_y;
											m_render_from_edit = true;
										}
									}
								}
							}
						}

						if (m_selected_brush.IsPickingBrushFromLayout() == false && (m_selected_brush.tile_brush != nullptr && m_selected_brush.tile_brush->texture == nullptr))
						{
							m_selected_brush.Clear();
						}

						if (m_selected_brush.HasBrushSelected())
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

						if (selected_collision_tile_index != -1)
						{
							const int collision_tile_origin_x = (static_cast<int>(selected_collision_tile_index) % rom::SplineCullingTable::grid_dimensions.x) * rom::SplineCullingTable::cell_dimensions.x;
							const int collision_tile_origin_y = (static_cast<int>(selected_collision_tile_index) / rom::SplineCullingTable::grid_dimensions.x) * rom::SplineCullingTable::cell_dimensions.y;

							ImGui::GetWindowDrawList()->AddRect(
								ImVec2{ static_cast<float>(screen_origin.x + collision_tile_origin_x), static_cast<float>(screen_origin.y + collision_tile_origin_y) },
								ImVec2{ static_cast<float>(screen_origin.x + collision_tile_origin_x + rom::SplineCullingTable::cell_dimensions.x), static_cast<float>(screen_origin.y + collision_tile_origin_y + rom::SplineCullingTable::cell_dimensions.y) },
								ImGui::GetColorU32(ImVec4{ 64,64,64,255 }), 0, ImDrawFlags_None, 1.0f);
						}

						if (current_layer_settings.hover_game_objects)
						{
							for (std::unique_ptr<UIGameObject>& game_obj : m_preview_game_objects)
							{
								ImGui::SetCursorPos(ImVec2{ origin.x + game_obj->pos.x, origin.y + game_obj->pos.y });
								ImGui::Dummy(game_obj->dimensions);
								if (ImGui::IsPopupOpen("obj_popup") == false && ImGui::IsMouseHoveringRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()))
								{
									m_working_game_obj.reset();
									ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImGui::GetColorU32(ImVec4{ 0,192,0,255 }), 1.0f, 0, 2);

									if (current_layer_settings.visibility_culling)
									{
										rom::AnimatedObjectCullingTable anim_obj_table = rom::AnimatedObjectCullingTable::LoadFromROM(m_owning_ui.GetROM(), m_owning_ui.GetROM().ReadUint32(m_level_data_offsets.camera_activation_sector_anim_obj_ids));
										for (Uint32 sector_index = 0; sector_index < anim_obj_table.cells.size() - 1; ++sector_index)
										{
											const rom::AnimatedObjectCullingCell& cell = anim_obj_table.cells[sector_index];
											for (const Uint16 obj_id : cell.obj_ids)
											{
												if (obj_id == game_obj->obj_definition.instance_id)
												{
													ImGui::GetWindowDrawList()->AddRect(
														ImVec2{ static_cast<float>(screen_origin.x + cell.bbox.min.x), static_cast<float>(screen_origin.y + cell.bbox.min.y) },
														ImVec2{ static_cast<float>(screen_origin.x + cell.bbox.max.x), static_cast<float>(screen_origin.y + cell.bbox.max.y) },
														ImGui::GetColorU32(ImVec4{ 255,255,255,255 }), 0, ImDrawFlags_None, 1.0f);
												}
											}
										}
									}

									if (current_layer_settings.collision_culling && m_level_data_offsets.collision_tile_obj_ids.offset != 0)
									{
										rom::GameObjectCullingTable game_obj_table = rom::GameObjectCullingTable::LoadFromROM(m_owning_ui.GetROM(), m_level_data_offsets.collision_tile_obj_ids.offset);
										for (Uint32 sector_index = 0; sector_index < game_obj_table.cells.size() - 1; ++sector_index)
										{
											const rom::GameObjectCullingCell& cell = game_obj_table.cells[sector_index];
											for (const Uint16 obj_id : cell.obj_ids)
											{
												if (obj_id == game_obj->obj_definition.instance_id)
												{
													ImGui::GetWindowDrawList()->AddRect(
														ImVec2{ static_cast<float>(screen_origin.x + cell.bbox.min.x), static_cast<float>(screen_origin.y + cell.bbox.min.y) },
														ImVec2{ static_cast<float>(screen_origin.x + cell.bbox.max.x), static_cast<float>(screen_origin.y + cell.bbox.max.y) },
														ImGui::GetColorU32(ImVec4{ 255,0,255,255 }), 0, ImDrawFlags_None, 2.0f);
												}
											}
										}
									}

									if (m_working_spline.has_value() == false && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
									{
										ImGui::OpenPopup("obj_popup");
										m_working_game_obj.emplace();
										m_working_game_obj->destination = game_obj.get();
										m_working_game_obj->game_obj = game_obj->obj_definition;
									}
									else if (m_working_spline.has_value() == false && ImGui::BeginTooltip())
									{
										ImGui::Text("Sprite Table: 0x%04X", game_obj->sprite_table_address);
										ImGui::Text("Instance ID: 0x%02X", game_obj->obj_definition.instance_id);
										ImGui::Image((ImTextureID)game_obj->ui_sprite->texture.get(), ImVec2(static_cast<float>(game_obj->ui_sprite->texture->w) * 2.0f, static_cast<float>(game_obj->ui_sprite->texture->h) * 2.0f));

										ImGui::Text("Type ID: 0x%02X", game_obj->obj_definition.type_id);
										ImGui::Text("Instance ID: 0x%02X", game_obj->obj_definition.instance_id);
										ImGui::Text("Unk 1: %d", game_obj->obj_definition.unk_1);
										ImGui::Text("Unk 2: %d", game_obj->obj_definition.unk_2);
										ImGui::Text("X: 0x%04X", game_obj->obj_definition.x_pos);
										ImGui::Text("Y: 0x%04X", game_obj->obj_definition.y_pos);
										ImGui::Text("Width: 0x%04X", game_obj->obj_definition.collision_width);
										ImGui::Text("Height: 0x%04X", game_obj->obj_definition.collision_height);
										ImGui::Text("Anim Definition: 0x%08X", game_obj->obj_definition.animation_definition);
										ImGui::Text("Anim Ptr: 0x%08X", game_obj->obj_definition.animation_ptr);
										ImGui::Text("Flags: 0x%04X", game_obj->obj_definition.flags);
										ImGui::Text("Flip X: %d", game_obj->obj_definition.FlipX());
										ImGui::Text("Flip Y: %d", game_obj->obj_definition.FlipY());

										ImGui::EndTooltip();
									}
								}
							}
						}

						if (m_working_spline && m_working_spline->dest_spline_point != nullptr)
						{
							if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
							{
								m_working_spline->dest_spline_point->x = static_cast<int>(ImGui::GetMousePos().x - screen_origin.x);
								m_working_spline->dest_spline_point->y = static_cast<int>(ImGui::GetMousePos().y - screen_origin.y);
							}
							else
							{
								ImGui::OpenPopup("spline_popup");
								m_working_spline->dest_spline_point = nullptr;
							}
						}

						if (m_working_game_obj.has_value() == false && (m_working_spline.has_value() == false || m_working_spline->dest_spline_point != nullptr))
						{
							ImGui::CloseCurrentPopup();
						}

						if (m_working_game_obj && ImGui::BeginPopup("obj_popup"))
						{
							const auto origin_offset = m_working_game_obj->destination->ui_sprite != nullptr ? m_working_game_obj->destination->ui_sprite->sprite->GetOriginOffsetFromMinBounds() : Point{ 0,0 };
							int pos[2] = { static_cast<int>(m_working_game_obj->game_obj.x_pos), static_cast<int>(m_working_game_obj->game_obj.y_pos) };
							if (ImGui::InputInt2("Object Pos", pos))
							{
								m_working_game_obj->game_obj.x_pos = pos[0];
								m_working_game_obj->game_obj.y_pos = pos[1];
							}

							if (ImGui::Button("Confirm"))
							{
								m_working_game_obj->destination->obj_definition = m_working_game_obj->game_obj;
								m_working_game_obj->destination->obj_definition.SaveToROM(m_owning_ui.GetROM());
								m_render_from_edit = true;
								m_working_game_obj.reset();
								ImGui::CloseCurrentPopup();
							}

							if (ImGui::Button("Cancel"))
							{
								ImGui::CloseCurrentPopup();
							}
							ImGui::EndPopup();
						}

						if (ImGui::BeginPopup("spline_popup"))
						{
							if (m_working_spline.has_value() == false || m_working_spline->dest_spline_point != nullptr)
							{
								ImGui::CloseCurrentPopup();
							}
							else
							{
								ImGui::Text("Obj Flags: 0x%04X", m_working_spline->spline.object_type_flags);
								int working_info = m_working_spline->spline.extra_info;
								ImGui::InputInt("Extra Info: 0x%04X", &working_info, 1,1,ImGuiInputTextFlags_CharsHexadecimal);
								m_working_spline->spline.extra_info = (0x0000FFFF & working_info);

								int spline_min[2] = { static_cast<int>(m_working_spline->spline.spline_vector.min.x), static_cast<int>(m_working_spline->spline.spline_vector.min.y) };
								int spline_max[2] = { static_cast<int>(m_working_spline->spline.spline_vector.max.x), static_cast<int>(m_working_spline->spline.spline_vector.max.y) };

								ImGui::InputInt2("Spline Min", spline_min);
								ImGui::InputInt2("Spline Max", spline_max);

								m_working_spline->spline.spline_vector.min.x = spline_min[0];
								m_working_spline->spline.spline_vector.min.y = spline_min[1];
								m_working_spline->spline.spline_vector.max.x = spline_max[0];
								m_working_spline->spline.spline_vector.max.y = spline_max[1];

								bool teleporter = m_working_spline->spline.object_type_flags & 0x1000;
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

								bool ring = m_working_spline->spline.object_type_flags & 0x2000;
								if (ImGui::Checkbox("Ring / Collectible", &ring))
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

								bool round_bumper = m_working_spline->spline.object_type_flags & 0x8000;
								if (ImGui::Checkbox("Radial Collision", &round_bumper))
								{
									if (round_bumper)
									{
										m_working_spline->spline.object_type_flags |= 0x8000;
									}
									else
									{
										m_working_spline->spline.object_type_flags &= ~0x8000;
									}
								}

								bool can_walk = m_working_spline->spline.object_type_flags & 0x0080;
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

								bool trigger_slide = m_working_spline->spline.object_type_flags & 0x0100;
								if (ImGui::Checkbox("Trigger Slide Anim", &trigger_slide))
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

								bool standard_bumper = m_working_spline->spline.object_type_flags & 0x4000;
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

								if (ImGui::Button("Confirm"))
								{
									*m_working_spline->destination = m_working_spline->spline;
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
							}
							ImGui::EndPopup();
						}

						if (ImGui::IsPopupOpen("spline_popup") == false && m_working_spline.has_value() && m_working_spline->dest_spline_point == nullptr)
						{
							m_working_spline.reset();
						}
					}
				}

				ImGuiWindow* tile_area = ImGui::GetCurrentContext()->CurrentWindow;
				if (tile_area != nullptr)
				{
					const bool is_hovering_tile_area = (ImGui::GetCurrentContext()->HoveredWindow == tile_area);

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
							tile_area->Scroll.x = scroll_start_pos.x + (drag_start_pos.x - ImGui::GetMousePos().x);
							tile_area->Scroll.y = scroll_start_pos.y + (drag_start_pos.y - ImGui::GetMousePos().y);
						}
					}
					else
					{
						is_dragging = false;
					}

					int scroll_multiplier = 1;

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
				}
				ImGui::EndChild();
			}
			ImGui::EndGroup();

			if (m_selected_brush.IsActive() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
			{
				if (m_selected_brush.was_picked_from_layout == true)
				{
					m_selected_brush.was_picked_from_layout = false;
					m_selected_brush.is_picking_from_layout = true;
					m_selected_brush.tile_brush = nullptr;
				}
				else
				{
					m_selected_brush.Clear();
				}
			}
		}
		ImGui::End();
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
				const Uint32 BGTilesetOffsets = m_owning_ui.GetROM().ReadUint32(m_level_data_offsets.background_tileset);
				const Uint32 BGTilesetLayouts = m_owning_ui.GetROM().ReadUint32(m_level_data_offsets.background_tile_layout);
				const Uint32 BGTilesetBrushes = m_owning_ui.GetROM().ReadUint32(m_level_data_offsets.background_tile_brushes);
				const Uint32 FGTilesetLayouts = m_owning_ui.GetROM().ReadUint32(m_level_data_offsets.foreground_tile_layout);


				const Uint16 LevelDimensionsX = m_owning_ui.GetROM().ReadUint16(m_level_data_offsets.tile_layout_width);
				const Uint16 LevelDimensionsY = m_owning_ui.GetROM().ReadUint16(m_level_data_offsets.tile_layout_height);

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
				sprintf_s(levelname_buffer, "level_%d", m_level->level_index);
				request.layout_type_name = levelname_buffer;
				request.layout_layout_name = "bg";

				m_working_palette_set = *rom::PaletteSet::LoadFromROM(m_owning_ui.GetROM(), m_level_data_offsets.palette_set);
				m_level->m_tile_layers[0].palette_set = *rom::PaletteSet::LoadFromROM(m_owning_ui.GetROM(), m_level_data_offsets.palette_set);
				request.store_tileset = &m_level->m_tile_layers[0].tileset;
				request.store_layout = &m_level->m_tile_layers[0].tile_layout;

				m_tile_layout_render_requests.emplace_back(std::move(request));
			}

			{
				const auto& buffer = m_owning_ui.GetROM().m_buffer;
				const rom::LevelDataOffsets level_data_offsets{ m_level->level_index };
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
				sprintf_s(levelname_buffer, "level_%d", m_level->level_index);
				request.layout_type_name = levelname_buffer;
				request.layout_layout_name = "fg";

				m_level->m_tile_layers[1].palette_set = *rom::PaletteSet::LoadFromROM(m_owning_ui.GetROM(), m_level_data_offsets.palette_set);
				request.store_tileset = &m_level->m_tile_layers[1].tileset;
				request.store_layout = &m_level->m_tile_layers[1].tile_layout;;

				m_tile_layout_render_requests.emplace_back(std::move(request));
			}
		}
		break;

		case RenderRequestType::OPTIONS:
		{
			m_level.reset();

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
				m_level.reset();
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
			m_level.reset();
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
			m_level.reset();
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
			m_level.reset();
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
			m_level.reset();
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
			m_level.reset();
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

	void EditorTileLayoutViewer::TestCollisionCullingResults() const
	{
		// Spline culling unit test
		SplineManager boogaloo;
		boogaloo.LoadFromSplineCullingTable(m_spline_manager.GenerateSplineCullingTable());

		const auto rom_table = rom::SplineCullingTable::LoadFromROM(m_owning_ui.GetROM(), m_owning_ui.GetROM().ReadUint32(m_level_data_offsets.collision_data_terrain));
		const auto og_table = m_spline_manager.GenerateSplineCullingTable();
		const auto new_table = boogaloo.GenerateSplineCullingTable();

		for (size_t i = 0; i < rom_table.cells.size(); ++i)
		{
			const rom::SplineCullingCell& rom_cell = rom_table.cells[i];
			const rom::SplineCullingCell& og_cell = og_table.cells[i];
			const rom::SplineCullingCell& new_cell = og_table.cells[i];

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
					if (std::none_of(std::begin(og_cell.splines), std::end(og_cell.splines), [&spline](const rom::CollisionSpline& _spline)
						{
							return _spline == spline || spline.extra_info == 0x0006;
						}))
					{
						// Did we lose this spline entirely, or was this some redunant data?

						assert(std::any_of(std::begin(og_table.cells), std::end(og_table.cells),
							[&spline](const rom::SplineCullingCell& _cell)
							{
								return std::any_of(std::begin(_cell.splines), std::end(_cell.splines),
									[&spline](const rom::CollisionSpline& _spline)
									{
										return _spline == spline;
									});
							}));
					}
				}
			}

			if ((og_cell.splines.size() == new_cell.splines.size() && og_cell.splines.size() == rom_cell.splines.size()) == false)
			{
				break;
			}

		}
	}
}