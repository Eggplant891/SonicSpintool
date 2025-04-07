#include "ui_tile_editor.h"

#include "editor/level.h"
#include "imgui.h"
#include "render.h"

namespace spintool
{
	void EditorTileEditor::RenderBrush()
	{
		m_brush_sprite = {};
		for (size_t i = 0; i < m_target_brush->tiles.size(); ++i)
		{
			rom::TileInstance& tile = m_target_brush->tiles[i];
			std::shared_ptr<rom::SpriteTile> sprite_tile = m_tile_layer->tileset->CreateSpriteTileFromTile(tile.tile_index);

			if (sprite_tile == nullptr)
			{
				break;
			}

			const size_t current_brush_offset = i;

			sprite_tile->x_offset = static_cast<Sint16>(current_brush_offset % m_target_brush->BrushWidth()) * rom::TileSet::s_tile_width;
			sprite_tile->y_offset = static_cast<Sint16>((current_brush_offset - (current_brush_offset % m_target_brush->BrushWidth())) / m_target_brush->BrushWidth()) * rom::TileSet::s_tile_height;

			sprite_tile->blit_settings.flip_horizontal = tile.is_flipped_horizontally;
			sprite_tile->blit_settings.flip_vertical = tile.is_flipped_vertically;

			sprite_tile->blit_settings.palette = m_tile_layer->palette_set.palette_lines.at(tile.palette_line);
			m_brush_sprite.sprite_tiles.emplace_back(std::move(sprite_tile));
		}
		m_brush_sprite.num_tiles = static_cast<Uint16>(m_brush_sprite.sprite_tiles.size());
		SDLSurfaceHandle new_surface{ SDL_CreateSurface(m_brush_sprite.GetBoundingBox().Width(), m_brush_sprite.GetBoundingBox().Height(), SDL_PIXELFORMAT_RGBA32) };
		SDL_SetSurfaceColorKey(new_surface.get(), true, SDL_MapRGBA(SDL_GetPixelFormatDetails(new_surface->format), nullptr, 0, 0, 0, 0));
		SDL_ClearSurface(new_surface.get(), 0.0f, 0, 0, 0);
		m_brush_sprite.RenderToSurface(new_surface.get());
		SDL_Surface* the_surface = new_surface.get();
		m_brush_preview = TileBrushPreview{ std::move(new_surface), Renderer::RenderToTexture(the_surface), static_cast<Uint32>(m_brush_index) };
	}
	void EditorTileEditor::Update()
	{
		if (IsOpen() == false)
		{
			return;
		}

		ImGui::SetNextWindowSize(ImVec2(800, 800), ImGuiCond_Appearing);
		if (ImGui::Begin("Tile Editor", &m_visible, ImGuiWindowFlags_NoSavedSettings))
		{
			if (m_target_brush == nullptr)
			{
				ImGui::Text("<INVALID BRUSH TARGET>");
			}

			if (ImGui::Button("Close Window"))
			{
				m_visible = false;
			}

			const float max_zoom = 16.0f;
			float zoom = max_zoom;

			const ImVec2 tile_origin_screen = ImGui::GetCursorScreenPos();

			for (; zoom >= 1.0f; zoom = zoom / 2.0f)
			{
				ImGui::Image((ImTextureID)m_brush_preview.texture.get(), ImVec2{ 32 * zoom ,  32 * zoom }); ImGui::SameLine();
			}

			for (unsigned int grid_y = 0; grid_y < m_target_brush->BrushHeight(); ++grid_y)
			{
				for (unsigned int grid_x = 0; grid_x < m_target_brush->BrushWidth(); ++grid_x)
				{
					ImGui::GetWindowDrawList()->AddRect(
						ImVec2{ tile_origin_screen.x + (grid_x * 8 * max_zoom), tile_origin_screen.y + (grid_y * 8 * max_zoom) },
						ImVec2{ tile_origin_screen.x + ((grid_x + 1) * 8 * max_zoom), tile_origin_screen.y + ((grid_y + 1) * 8 * max_zoom) }, ImGui::GetColorU32(ImVec4(255,0,0,255)));
				}
			}
		}
		ImGui::End();
	}

	EditorTileEditor::EditorTileEditor(EditorUI& owning_ui, TileLayer& tile_layer, Uint32 brush_index)
		: EditorWindowBase(owning_ui)
		, m_tile_layer(&tile_layer)
		, m_brush_index(brush_index)
	{
		m_target_brush = m_tile_layer->tile_layout->tile_brushes[m_brush_index].get();
		RenderBrush();
	}

}
