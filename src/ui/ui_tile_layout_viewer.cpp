#include "ui/ui_tile_layout_viewer.h"

#include "ui/ui_editor.h"

#include "rom/sprite.h"
#include "rom/tileset.h"
#include "rom/tile_layout.h"
#include "rom/game_objects/game_object_flipper.h"
#include "rom/game_objects/game_object_ring.h"

#include "SDL3/SDL_image.h"
#include "imgui.h"
#include <memory>
#include <cassert>


namespace spintool
{
	void EditorTileLayoutViewer::Update()
	{
		if (IsOpen() == false)
		{
			return;
		}

		if (ImGui::Begin("Tile Layout Viewer", &m_visible, ImGuiWindowFlags_HorizontalScrollbar))
		{
			static std::vector<RenderTileLayoutRequest> s_tile_layout_render_requests;
			static std::vector<rom::FlipperInstance> s_flipper_instances;
			static std::vector<rom::RingInstance> s_ring_instances;
			bool render_preview = false;
			static SDLSurfaceHandle LevelFlipperSpriteSurface;
			static SDLPaletteHandle LevelFlipperPalette;
			static SDLSurfaceHandle LevelRingSpriteSurface;
			static SDLPaletteHandle LevelRingPalette;
			static std::shared_ptr<rom::PaletteSet> LevelPaletteSet;

			static int level_index = 0;
			ImGui::SliderInt("###Level Index", &level_index, 0, 3);


			bool render_both = ImGui::Button("Preview Level");
			ImGui::SameLine();
			bool export_both = ImGui::Button("Export level");

			bool render_bg = ImGui::Button("Preview level BG");
			ImGui::SameLine();
			bool export_bg = ImGui::Button("Export level BG");

			bool render_fg = ImGui::Button("Preview level FG");
			ImGui::SameLine();
			bool export_fg = ImGui::Button("Export level FG");

			bool render_flippers = false;
			bool render_rings = false;

			if (render_both)
			{
				render_fg = true;
				render_bg = true;
				render_flippers = true;
				render_rings = true;
			}

			if (export_both)
			{
				export_fg = true;
				export_bg = true;
				render_flippers = true;
				render_rings = true;
			}

			if (render_bg || export_bg)
			{
				const auto& buffer = m_owning_ui.GetROM().m_buffer;
				const Uint32 BGTilesetOffsets = m_owning_ui.GetROM().ReadUint32(0x000bfbf0 + (4 * level_index));
				const Uint32 BGTilesetLayouts = m_owning_ui.GetROM().ReadUint32(0x000bfc10 + (4 * level_index));
				const Uint32 BGTilesetBrushes = m_owning_ui.GetROM().ReadUint32(0x000bfc30 + (4 * level_index));

				LevelPaletteSet = rom::PaletteSet::LoadFromROM(m_owning_ui.GetROM(), 0x000bfc50 + (8 * level_index));

				const Uint16 LevelDimensionsX = m_owning_ui.GetROM().ReadUint16(0x000bfc70 + (4 * level_index));
				const Uint16 LevelDimensionsY = m_owning_ui.GetROM().ReadUint16(0x000bfc72 + (4 * level_index));

				RenderTileLayoutRequest request;

				request.tileset_address = BGTilesetOffsets;
				request.tile_brushes_address = BGTilesetBrushes;
				request.tile_brushes_address_end = BGTilesetLayouts;

				request.tile_layout_width = LevelDimensionsX / (rom::TileBrush<4,4>::s_brush_width * rom::TileSet::s_tile_width);
				request.tile_layout_height = LevelDimensionsY / (rom::TileBrush<4,4>::s_brush_height * rom::TileSet::s_tile_height);

				request.tile_layout_address = BGTilesetLayouts;
				request.tile_layout_address_end = request.tile_layout_address + (request.tile_layout_width * request.tile_layout_height * 2);

				request.is_chroma_keyed = false;
				request.compression_algorithm = CompressionAlgorithm::SSC;
				
				s_tile_layout_render_requests.emplace_back(std::move(request));
			}

			if (render_fg || export_fg)
			{
				const auto& buffer = m_owning_ui.GetROM().m_buffer;
				const Uint32 FGTilesetOffsets = m_owning_ui.GetROM().ReadUint32(0x000bfbe0 + (4 * level_index));
				const Uint32 FGTilesetLayouts = m_owning_ui.GetROM().ReadUint32(0x000bfc00 + (4 * level_index));
				const Uint32 FGTilesetBrushes = m_owning_ui.GetROM().ReadUint32(0x000bfc20 + (4 * level_index));

				LevelPaletteSet = rom::PaletteSet::LoadFromROM(m_owning_ui.GetROM(), 0x000bfc50 + (8 * level_index));

				const Uint16 LevelDimensionsX = m_owning_ui.GetROM().ReadUint16(0x000bfc70 + (4 * level_index));
				const Uint16 LevelDimensionsY = m_owning_ui.GetROM().ReadUint16(0x000bfc72 + (4 * level_index));

				RenderTileLayoutRequest request;

				request.tileset_address = FGTilesetOffsets;
				request.tile_brushes_address = FGTilesetBrushes;
				request.tile_brushes_address_end = FGTilesetLayouts;

				request.tile_layout_width = LevelDimensionsX / (rom::TileBrush<4,4>::s_brush_width * rom::TileSet::s_tile_width);
				request.tile_layout_height = LevelDimensionsY / (rom::TileBrush<4,4>::s_brush_height * rom::TileSet::s_tile_height);

				request.tile_layout_address = FGTilesetLayouts;
				request.tile_layout_address_end = request.tile_layout_address + (request.tile_layout_width * request.tile_layout_height * 2);

				request.is_chroma_keyed = true;
				request.compression_algorithm = CompressionAlgorithm::SSC;

				s_tile_layout_render_requests.emplace_back(std::move(request));
			}

			bool preview_options = ImGui::Button("Preview Options Menu");
			ImGui::SameLine();
			bool preview_intro = ImGui::Button("Preview Intro Cutscene");

			bool export_options = ImGui::Button("Export Options Menu");
			ImGui::SameLine();
			bool export_intro = ImGui::Button("Export Intro Cutscene");
			if (preview_options || export_options)
			{
				RenderTileLayoutRequest request;
				
				request.tileset_address = 0x000BDD2E;
				request.tile_brushes_address = 0x000BDFBC;
				request.tile_brushes_address_end = 0x000BE1BC;
				request.tile_layout_address = 0x000BE1BC;
				request.tile_layout_address_end = 0x000BE248;

				request.tile_layout_width = 0xA;
				request.tile_layout_height = 0x7;

				request.is_chroma_keyed = false;
				request.compression_algorithm = CompressionAlgorithm::SSC;

				LevelPaletteSet = m_owning_ui.GetROM().GetOptionsScreenPaletteSet();

				s_tile_layout_render_requests.emplace_back(std::move(request));
			}

			
			if (preview_intro || export_intro)
			{
				RenderTileLayoutRequest request;

				request.tileset_address = 0x000A3124 + 2;
				request.tile_brushes_address = 0x000A1984;
				request.tile_brushes_address_end = 0x000A220C;
				request.tile_layout_address = 0x000A14F8;
				request.tile_layout_address_end = 0x000A14F8 + (0xE * 0xC);

				request.tile_layout_width = 0xE;
				request.tile_layout_height = 0xC;

				request.is_chroma_keyed = false;
				request.show_brush_previews = false;
				request.compression_algorithm = CompressionAlgorithm::BOOGALOO;

				LevelPaletteSet = m_owning_ui.GetROM().GetIntroCutscenePaletteSet();

				s_tile_layout_render_requests.emplace_back(std::move(request));
			}

			if (render_flippers)
			{
				const static size_t flippers_ptr_table_offset = 0x000C08BE;
				const static size_t flippers_count_table_offset = 0x000C08EE;
				const static size_t flipper_palette[4] =
				{
					0x1,
					0x0,
					0x0,
					0x0
				};
				const static size_t flipper_sprite_offset[4] =
				{
					0x00017E82,
					0x0002594A,
					0x0002E05C,
					0x00035692
				};

				std::shared_ptr<const rom::Sprite> LevelFlipperSprite = rom::Sprite::LoadFromROM(m_owning_ui.GetROM(), flipper_sprite_offset[level_index]);
				LevelFlipperSpriteSurface = SDLSurfaceHandle{ SDL_CreateSurface(44, 31, SDL_PIXELFORMAT_INDEX8) };
				LevelFlipperPalette = Renderer::CreateSDLPalette(*LevelPaletteSet->palette_lines[flipper_palette[level_index]].get());
				SDL_SetSurfacePalette(LevelFlipperSpriteSurface.get(), LevelFlipperPalette.get());
				SDL_ClearSurface(LevelFlipperSpriteSurface.get(), 0.0f, 0.0f, 0.0f, 0.0f);
				SDL_SetSurfaceColorKey(LevelFlipperSpriteSurface.get(), true, SDL_MapRGBA(SDL_GetPixelFormatDetails(LevelFlipperSpriteSurface->format), nullptr, 0, 0, 0, 0));
				LevelFlipperSprite->RenderToSurface(LevelFlipperSpriteSurface.get());

				const Uint32 flippers_table_begin = m_owning_ui.GetROM().ReadUint32(flippers_ptr_table_offset + (4 * level_index));
				const Uint32 num_flippers = m_owning_ui.GetROM().ReadUint16(flippers_count_table_offset + (2 * level_index));

				s_flipper_instances.clear();
				s_flipper_instances.reserve(num_flippers);

				size_t current_table_offset = flippers_table_begin;

				for (size_t i = 0; i < num_flippers; ++i)
				{
					s_flipper_instances.emplace_back(rom::FlipperInstance::LoadFromROM(m_owning_ui.GetROM(), current_table_offset));
					current_table_offset = s_flipper_instances.back().rom_data.rom_offset_end;
				}
			}

			if (render_rings)
			{
				const static size_t ring_sprite_offset = 0x0000F6D8;
				const static size_t level_rings_count[4] =
				{
					0x3A,
					0x43,
					0x51,
					0x3E
				};

				const static size_t rings_positions_table_offset[4] =
				{
					0x000C3854,
					0x000C1D70,
					0x000C60B2,
					0x000C49CC
				};

				std::shared_ptr<const rom::Sprite> LevelRingSprite = rom::Sprite::LoadFromROM(m_owning_ui.GetROM(), ring_sprite_offset);
				LevelRingSpriteSurface = SDLSurfaceHandle{ SDL_CreateSurface(LevelRingSprite->GetBoundingBox().Width(), LevelRingSprite->GetBoundingBox().Height(), SDL_PIXELFORMAT_INDEX8) };
				LevelRingPalette = Renderer::CreateSDLPalette(*LevelPaletteSet->palette_lines[3].get());
				SDL_SetSurfacePalette(LevelRingSpriteSurface.get(), LevelRingPalette.get());
				SDL_ClearSurface(LevelRingSpriteSurface.get(), 0.0f, 0.0f, 0.0f, 0.0f);
				SDL_SetSurfaceColorKey(LevelRingSpriteSurface.get(), true, SDL_MapRGBA(SDL_GetPixelFormatDetails(LevelRingSpriteSurface->format), nullptr, 0, 0, 0, 0));
				LevelRingSprite->RenderToSurface(LevelRingSpriteSurface.get());

				s_ring_instances.clear();
				s_ring_instances.reserve(level_rings_count[level_index]);

				size_t current_table_offset = rings_positions_table_offset[level_index];

				for (size_t i = 0; i < level_rings_count[level_index]; ++i)
				{
					s_ring_instances.emplace_back(rom::RingInstance::LoadFromROM(m_owning_ui.GetROM(), current_table_offset));
					current_table_offset = s_ring_instances.back().rom_data.rom_offset_end;
				}
			}

			const size_t brush_width =  rom::TileBrush<4, 4>::s_brush_width * rom::TileSet::s_tile_width;
			const size_t brush_height = rom::TileBrush<4, 4>::s_brush_height * rom::TileSet::s_tile_height;

			const bool will_be_rendering_preview = s_tile_layout_render_requests.empty() == false;
			SDLSurfaceHandle layout_preview_surface;
			if (will_be_rendering_preview)
			{
				const RenderTileLayoutRequest& request = s_tile_layout_render_requests.front();
				layout_preview_surface = SDLSurfaceHandle{ SDL_CreateSurface(brush_width * request.tile_layout_width, brush_height * request.tile_layout_height, SDL_PIXELFORMAT_RGBA32) };
				SDL_ClearSurface(layout_preview_surface.get(), 0.0f, 0.0f, 0.0f, 0.0f);
			}
			
			while (s_tile_layout_render_requests.empty() == false)
			{
				const RenderTileLayoutRequest& request = s_tile_layout_render_requests.front();
				if (request.compression_algorithm == CompressionAlgorithm::SSC)
				{
					m_tileset = rom::TileSet::LoadFromROM(m_owning_ui.GetROM(), request.tileset_address).tileset;
					m_tile_layout = rom::TileLayout::LoadFromROM(m_owning_ui.GetROM(), request.tile_brushes_address, request.tile_brushes_address_end, request.tile_layout_address, request.tile_layout_address_end);
				}
				else if (request.compression_algorithm == CompressionAlgorithm::BOOGALOO)
				{
					m_tileset = rom::TileSet::LoadFromROMSecondCompression(m_owning_ui.GetROM(), request.tileset_address).tileset;
					m_tile_layout = rom::TileLayout::LoadFromROM(m_owning_ui.GetROM(), *m_tileset, request.tile_layout_address, request.tile_layout_address_end);
				}
				std::shared_ptr<const rom::Sprite> tileset_sprite = m_tileset->CreateSpriteFromTile(0);
				std::shared_ptr<const rom::Sprite> tile_layout_sprite = std::make_unique<rom::Sprite>();

				std::vector<rom::Sprite> brushes;
				m_tile_brushes_previews.clear();

				for (size_t brush_index = 0; brush_index < m_tile_layout->tile_brushes.size(); ++brush_index)
				{
					rom::TileBrushBase& brush = *m_tile_layout->tile_brushes[brush_index].get();
					rom::Sprite& brush_sprite = brushes.emplace_back();
					for (size_t i = 0; i < brush.tiles.size(); ++i)
					{
						rom::TileInstance& tile = brush.tiles[i];
						std::shared_ptr<rom::SpriteTile> sprite_tile = m_tileset->CreateSpriteTileFromTile(tile.tile_index);

						if (sprite_tile == nullptr)
						{
							break;
						}

						const size_t current_brush_offset = i;

						sprite_tile->x_offset = static_cast<Sint16>(current_brush_offset % brush.BrushWidth()) * rom::TileSet::s_tile_width;
						sprite_tile->y_offset = static_cast<Sint16>((current_brush_offset - (current_brush_offset % brush.BrushWidth())) / brush.BrushWidth()) * rom::TileSet::s_tile_height;

						sprite_tile->blit_settings.flip_horizontal = tile.is_flipped_horizontally;
						sprite_tile->blit_settings.flip_vertical = tile.is_flipped_vertically;

						if (LevelPaletteSet != nullptr)
						{
							sprite_tile->blit_settings.palette = LevelPaletteSet->palette_lines.at(tile.palette_line);
						}
						brush_sprite.sprite_tiles.emplace_back(std::move(sprite_tile));
					}
					brush_sprite.num_tiles = static_cast<Uint16>(brush_sprite.sprite_tiles.size());
					SDLSurfaceHandle new_surface{ SDL_CreateSurface(brush_sprite.GetBoundingBox().Width(), brush_sprite.GetBoundingBox().Height(), SDL_PIXELFORMAT_RGBA32) };
					SDL_SetSurfaceColorKey(new_surface.get(), request.is_chroma_keyed, SDL_MapRGBA(SDL_GetPixelFormatDetails(new_surface->format), nullptr, 0, 0, 0, 0));
					SDL_ClearSurface(new_surface.get(), 0.0f, 0.0f, 0.0f, 255.0f);
					brush_sprite.RenderToSurface(new_surface.get());
					SDL_Surface* the_surface = new_surface.get();
					m_tile_brushes_previews.emplace_back(TileBrushPreview{ std::move(new_surface), Renderer::RenderToTexture(the_surface) });
				}
				
				for (size_t i = 0; i < m_tile_layout->tile_brush_instances.size(); ++i)
				{
					const auto tile_brush_index = m_tile_layout->tile_brush_instances[i].tile_brush_index;
					if (tile_brush_index >= m_tile_brushes_previews.size())
					{
						break;
					}

					SDLSurfaceHandle temp_surface{ SDL_DuplicateSurface(m_tile_brushes_previews[tile_brush_index].surface.get()) };
					if (m_tile_layout->tile_brush_instances[i].is_flipped_horizontally)
					{
						SDL_FlipSurface(temp_surface.get(), SDL_FLIP_HORIZONTAL);
					}

					if (m_tile_layout->tile_brush_instances[i].is_flipped_vertically)
					{
						SDL_FlipSurface(temp_surface.get(), SDL_FLIP_VERTICAL);
					}

					int x_off = static_cast<int>((i % request.tile_layout_width) * brush_width);
					int y_off = static_cast<int>(((i-(i % request.tile_layout_width)) / request.tile_layout_width) * brush_height);

					SDL_Rect target_rect{ x_off, y_off, brush_width, brush_height };
					SDL_SetSurfaceColorKey(temp_surface.get(), request.is_chroma_keyed, SDL_MapRGBA(SDL_GetPixelFormatDetails(temp_surface->format), nullptr, 0, 0, 0, 0));
					SDL_BlitSurface(temp_surface.get(), nullptr, layout_preview_surface.get(), &target_rect);
				}

				if (export_bg)
				{
					static char path_buffer[4096];

					sprintf_s(path_buffer, "spinball_level_%X02_BG.png", level_index);
					std::filesystem::path export_path = m_owning_ui.GetSpriteExportPath().append(path_buffer);
					assert(IMG_SavePNG(layout_preview_surface.get(), export_path.generic_u8string().c_str()));
				}

				if (export_fg)
				{
					static char path_buffer[4096];

					sprintf_s(path_buffer, "spinball_level_%X02_FG.png", level_index);
					std::filesystem::path export_path = m_owning_ui.GetSpriteExportPath().append(path_buffer);
					assert(IMG_SavePNG(layout_preview_surface.get(), export_path.generic_u8string().c_str()));
				}

				if (export_options)
				{
					std::filesystem::path export_path = m_owning_ui.GetSpriteExportPath().append("spinball_level_OPTIONS.png");
					assert(IMG_SavePNG(layout_preview_surface.get(), export_path.generic_u8string().c_str()));
				}
			
				s_tile_layout_render_requests.erase(std::begin(s_tile_layout_render_requests));
			}

			if (render_flippers)
			{
				for (size_t i = 0; i < s_flipper_instances.size(); ++i)
				{
					const rom::FlipperInstance& flipper = s_flipper_instances[i];

					SDLSurfaceHandle temp_surface{ SDL_DuplicateSurface(LevelFlipperSpriteSurface.get()) };
					int x_off = -24;
					if (flipper.is_x_flipped)
					{
						SDL_FlipSurface(temp_surface.get(), SDL_FLIP_HORIZONTAL);
						x_off = -20;
					}

					SDL_Rect target_rect{ flipper.x_pos + x_off, flipper.y_pos - 31, 44, 31 };
					SDL_SetSurfaceColorKey(temp_surface.get(), true, SDL_MapRGBA(SDL_GetPixelFormatDetails(temp_surface->format), nullptr, 0, 0, 0, 0));
					SDL_BlitSurface(temp_surface.get(), nullptr, layout_preview_surface.get(), &target_rect);
				}
			}

			if (render_rings)
			{
				for (size_t i = 0; i < s_ring_instances.size(); ++i)
				{
					const rom::RingInstance& ring = s_ring_instances[i];

					SDLSurfaceHandle temp_surface{ SDL_DuplicateSurface(LevelRingSpriteSurface.get()) };
					SDL_Rect target_rect{ ring.x_pos - 8, ring.y_pos - 16, 0x10, 0x10 };
					SDL_SetSurfaceColorKey(temp_surface.get(), true, SDL_MapRGBA(SDL_GetPixelFormatDetails(temp_surface->format), nullptr, 0, 0, 0, 0));
					SDL_BlitSurface(temp_surface.get(), nullptr, layout_preview_surface.get(), &target_rect);
				}
			}

			if (export_both)
			{
				static char path_buffer[4096];

				sprintf_s(path_buffer, "spinball_level_%X02_FullLayout.png", level_index);
				std::filesystem::path export_path = m_owning_ui.GetSpriteExportPath().append(path_buffer);
				assert(IMG_SavePNG(layout_preview_surface.get(), export_path.generic_u8string().c_str()));
			}

			if (will_be_rendering_preview)
			{
				m_tile_layout_preview = Renderer::RenderToTexture(layout_preview_surface.get());
			}

			constexpr size_t preview_brushes_per_row = 32;
			constexpr const float zoom = 1.0f;

			//if (ImGui::TreeNode("ROM Data Info"))
			//{
			//	ImGui::Text("Tileset Compressed Data:", tileset_address);
			//	ImGui::Text("Tileset Brushes: 0x%08X => 0x%08X", tile_brushes_address, tile_brushes_address_end);
			//	ImGui::Text("Tile Layout: 0x%08X => 0x%08X", tile_layout_address, tile_layout_address_end);
			//	ImGui::Text("Width: %d", tile_layout_width);
			//	ImGui::Text("Height: %d", tile_layout_height);
			//	ImGui::TreePop();
			//}

			if (ImGui::TreeNode("Brush Previews"))
			{
				if (m_tile_brushes_previews.empty() == false)
				{
					size_t i = 0;
					for (const TileBrushPreview& preview_brush : m_tile_brushes_previews)
					{
						if (preview_brush.texture != nullptr)
						{
							if (i % preview_brushes_per_row != 0)
							{
								ImGui::SameLine();
							}

							ImGui::Image((ImTextureID)preview_brush.texture.get(), ImVec2(static_cast<float>(preview_brush.texture->w) * zoom, static_cast<float>(preview_brush.texture->h) * zoom));
							if (ImGui::BeginItemTooltip())
							{
								ImGui::Text("Tile Index: 0x%02X", i);
								ImGui::EndTooltip();
							}
							++i;
						}
					}
				}
				ImGui::TreePop();
			}
			if (m_tile_layout_preview != nullptr)
			{
				ImGui::Image((ImTextureID)m_tile_layout_preview.get(), ImVec2(static_cast<float>(m_tile_layout_preview->w)* zoom, static_cast<float>(m_tile_layout_preview->h)* zoom));
			}
		}
		ImGui::End();
	}
}