#include "ui/ui_tile_layout_viewer.h"

#include "ui/ui_editor.h"

#include "rom/sprite.h"
#include "rom/tileset.h"
#include "rom/tile_layout.h"

#include "imgui.h"
#include <memory>

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
			size_t tileset_address = 0;
			size_t tile_brushes_address = 0;
			size_t tile_layout_address = 0;
			size_t tile_layout_size = 0;
			unsigned int tile_layout_width = 0;
			unsigned int tile_layout_height = 0;
			bool render_preview = false;
			const rom::PaletteSet* palette_set = nullptr;

			if (ImGui::Button("Preview Toxic Caves FG"))
			{
				tileset_address = 0x000394aa;
				tile_brushes_address = 0x0003E800;
				tile_layout_address = 0x000450E0;
				tile_layout_size = 0x000493e0 - tile_layout_address;
				tile_layout_width = 0x500 / (rom::TileBrush::s_brush_width * rom::TileSet::s_tile_width);
				tile_layout_height = 0x800 / (rom::TileBrush::s_brush_height * rom::TileSet::s_tile_height);
				palette_set = &m_owning_ui.GetROM().GetToxicCavesPaletteSet();
				render_preview = true;
			}

			if (ImGui::Button("Preview Toxic Caves BG"))
			{
				tileset_address = 0x0003dbb2;
				tile_brushes_address = 0x00049ea0;
				tile_layout_address = 0x0004b2a0;
				tile_layout_size = 0x0004C69F - tile_layout_address;
				tile_layout_width = 0x500 / (rom::TileBrush::s_brush_width * rom::TileSet::s_tile_width);
				tile_layout_height = 0x800 / (rom::TileBrush::s_brush_height * rom::TileSet::s_tile_height);
				palette_set = &m_owning_ui.GetROM().GetToxicCavesPaletteSet();
				render_preview = true;
			}

			ImGui::SameLine();
			if (ImGui::Button("Preview Options Menu"))
			{
				tileset_address = 0x000BDD2E;
				tile_brushes_address = 0x000BDFBC;
				tile_layout_address = 0x000BE1BC;
				tile_layout_size = 0x000BE248 - tile_layout_address;// 0x146;
				tile_layout_width = 0xA;
				tile_layout_height = 0xA;
				//tile_layout_size = 0x000BE248 - tile_brushes_address;// 0x146;
				render_preview = true;
				palette_set = &m_owning_ui.GetROM().GetOptionsScreenPaletteSet();
			}

			if (render_preview)
			{
				m_tileset = rom::TileSet::LoadFromROM(m_owning_ui.GetROM(), tileset_address);
				m_tile_layout = rom::TileLayout::LoadFromROM(m_owning_ui.GetROM(), tile_brushes_address, tile_layout_address, tile_layout_size);
				std::shared_ptr<const rom::Sprite> tileset_sprite = m_tileset->CreateSpriteFromTile(0);
				std::shared_ptr<const rom::Sprite> tile_layout_sprite = std::make_unique<rom::Sprite>();

				constexpr size_t tiles_per_tile_brush = 16;
				constexpr size_t tiles_per_brush_row = 4;

				std::vector<rom::Sprite> brushes;

				for (size_t brush_index = 0; brush_index < m_tile_layout->tile_brushes.size(); ++brush_index)
				{
					rom::TileBrush& brush = m_tile_layout->tile_brushes[brush_index];
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

						sprite_tile->x_offset = static_cast<Sint16>((current_brush_offset % tiles_per_brush_row) * rom::TileSet::s_tile_width);
						sprite_tile->y_offset = static_cast<Sint16>((current_brush_offset - (current_brush_offset % tiles_per_brush_row)) / tiles_per_brush_row) * rom::TileSet::s_tile_height;

						sprite_tile->blit_settings.flip_horizontal = tile.is_flipped_horizontally;
						sprite_tile->blit_settings.flip_vertical = tile.is_flipped_vertically;

						if (palette_set != nullptr)
						{
							sprite_tile->blit_settings.palette = palette_set->palette_lines.at(tile.palette_line);
						}
						brush_sprite.sprite_tiles.emplace_back(std::move(sprite_tile));
					}
					brush_sprite.num_tiles = static_cast<Uint16>(brush_sprite.sprite_tiles.size());
					SDLSurfaceHandle new_surface{ SDL_CreateSurface(brush_sprite.GetBoundingBox().Width(), brush_sprite.GetBoundingBox().Height(), SDL_PIXELFORMAT_RGBA32) };
					brush_sprite.RenderToSurface(new_surface.get());
					SDL_Surface* the_surface = new_surface.get();
					m_tile_brushes_previews.emplace_back(TileBrushPreview{ std::move(new_surface), Renderer::RenderToTexture(the_surface) });
				}

				constexpr size_t brush_width = rom::TileBrush::s_brush_width * rom::TileSet::s_tile_width;
				constexpr size_t brush_height = rom::TileBrush::s_brush_height * rom::TileSet::s_tile_height;

				SDLSurfaceHandle layout_preview_surface{ SDL_CreateSurface(brush_width * tile_layout_width, brush_height * tile_layout_height, SDL_PIXELFORMAT_RGBA32) };

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

					int x_off = static_cast<int>((i % tile_layout_width) * brush_width);
					int y_off = static_cast<int>(((i-(i % tile_layout_width)) / tile_layout_width) * brush_height);

					SDL_Rect target_rect{ x_off, y_off, brush_width, brush_height };
					SDL_BlitSurface(temp_surface.get(), nullptr, layout_preview_surface.get(), &target_rect);
				}

				m_tile_layout_preview = Renderer::RenderToTexture(layout_preview_surface.get());
			}

			constexpr size_t preview_brushes_per_row = 16;
			constexpr const float zoom = 1.0f;

			if (ImGui::TreeNode("Brush Previews"))
			{
				if (m_tile_brushes_previews.empty() == false)
				{
					int i = 0;
					for (const TileBrushPreview& preview_brush : m_tile_brushes_previews)
					{
						if (preview_brush.texture != nullptr)
						{
							if (i % preview_brushes_per_row != 0)
							{
								ImGui::SameLine();
							}

							ImGui::Image((ImTextureID)preview_brush.texture.get(), ImVec2(static_cast<float>(preview_brush.texture->w) * zoom, static_cast<float>(preview_brush.texture->h) * zoom));
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