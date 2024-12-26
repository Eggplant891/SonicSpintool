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

		if (ImGui::Begin("Tile Layout Viewer"))
		{
			if (ImGui::Button("Show Test Layout"))
			{
				m_tileset = rom::TileSet::LoadFromROM(m_owning_ui.GetROM(), 0x000394aa);
				m_tile_layout = rom::TileLayout::LoadFromROM(m_owning_ui.GetROM(), 0x0003e800);
				//std::shared_ptr<const rom::TileSet> tileset = rom::TileSet::LoadFromROM(m_owning_ui.GetROM(), 0x000BDD2E);
				//std::shared_ptr<rom::TileLayout> test_layout = rom::TileLayout::LoadFromROM(m_owning_ui.GetROM(), 0x000BDFBC);
				std::shared_ptr<const rom::Sprite> tileset_sprite = m_tileset->CreateSpriteFromTile(0);
				std::shared_ptr<const rom::Sprite> tile_layout_sprite = std::make_unique<rom::Sprite>();

				constexpr size_t tiles_per_tile_brush = 16;
				constexpr size_t brushes_per_row = 20;
				constexpr size_t tiles_per_brush_row = 4;

				//const rom::PaletteSet palette_set = m_owning_ui.GetROM().GetOptionsScreenPaletteSet();
				const rom::PaletteSet palette_set = m_owning_ui.GetROM().GetToxicCavesPaletteSet();

				for (size_t i = 0; i < m_tile_layout->tile_instances.size(); ++i)
				{
					rom::TileInstance& tile = m_tile_layout->tile_instances[i];
					std::shared_ptr<rom::SpriteTile> sprite_tile = m_tileset->CreateSpriteTileFromTile(tile.tile_index);

					const size_t current_tile_brush = i / tiles_per_tile_brush;
					const size_t current_brush_offset = i - (current_tile_brush * tiles_per_tile_brush);

					sprite_tile->x_offset = static_cast<Sint16>((current_tile_brush % brushes_per_row) * (rom::TileSet::s_tile_width * tiles_per_brush_row));
					sprite_tile->x_offset += static_cast<Sint16>((current_brush_offset % 4) * rom::TileSet::s_tile_width);
					sprite_tile->y_offset = static_cast<Sint16>(((current_tile_brush - (current_tile_brush % brushes_per_row)) / brushes_per_row) * (rom::TileSet::s_tile_height * tiles_per_brush_row));
					sprite_tile->y_offset += static_cast<Sint16>((current_brush_offset - (current_brush_offset % 4)) / 4) * rom::TileSet::s_tile_height;

					sprite_tile->blit_settings.flip_horizontal = tile.is_flipped_horizontally;
					sprite_tile->blit_settings.flip_vertical = tile.is_flipped_vertically;

					sprite_tile->blit_settings.palette = palette_set.palette_lines.at(tile.palette_line);

					const_cast<rom::Sprite*>(tile_layout_sprite.get())->sprite_tiles.emplace_back(std::move(sprite_tile));
				}
				const_cast<rom::Sprite*>(tile_layout_sprite.get())->num_tiles = static_cast<Uint16>(tile_layout_sprite->sprite_tiles.size());

				SDLSurfaceHandle new_surface{ SDL_CreateSurface(tile_layout_sprite->GetBoundingBox().Width(), tile_layout_sprite->GetBoundingBox().Height(), SDL_PIXELFORMAT_RGBA32) };
				tile_layout_sprite->RenderToSurface(new_surface.get());
				m_tile_layout_preview = Renderer::RenderToTexture(new_surface.get());
			}
		}
		ImGui::End();
	}
}