#include "ui/ui_tile_picker.h"

#include "render.h"
#include "rom/level.h"
#include "rom/sprite_tile.h"
#include "rom/tileset.h"

#include "imgui.h"
#include "ui/ui_palette_viewer.h"

namespace spintool
{

	TilePicker::TilePicker(EditorUI& owning_ui)
		: m_owning_ui(owning_ui)
	{

	}

	void TilePicker::RenderTileset()
	{
		if (m_tile_layer == nullptr)
		{
			return;
		}

		auto palette_line = current_palette_line;
		surface.reset();
		texture.reset();
		current_palette_line = palette_line;

		int max_x_size = 0;
		int max_y_size = 0;

		Uint32 offset = 0;
		for (Uint16 i = 0; i < m_tile_layer->tileset->num_tiles; ++i)
		{
			std::shared_ptr<rom::SpriteTile> sprite_tile = m_tile_layer->tileset->CreateSpriteTileFromTile(i);

			if (sprite_tile == nullptr)
			{
				break;
			}

			const size_t current_brush_offset = i;

			sprite_tile->x_offset = static_cast<Sint16>(current_brush_offset % picker_width) * rom::TileSet::s_tile_width;
			sprite_tile->y_offset = static_cast<Sint16>((current_brush_offset - (current_brush_offset % picker_width)) / picker_width) * rom::TileSet::s_tile_height;

			max_x_size = std::max(max_x_size, sprite_tile->x_offset + rom::TileSet::s_tile_width);
			max_y_size = std::max(max_x_size, sprite_tile->y_offset + rom::TileSet::s_tile_height);

			sprite_tile->blit_settings.flip_horizontal = false;
			sprite_tile->blit_settings.flip_vertical = false;

			sprite_tile->blit_settings.palette = m_tile_layer->palette_set.palette_lines.at(current_palette_line);
			tiles.emplace_back(std::move(sprite_tile));
		}

		surface = SDLSurfaceHandle{ SDL_CreateSurface(max_x_size, max_y_size, SDL_PIXELFORMAT_RGBA32) };
		SDL_SetSurfaceColorKey(surface.get(), true, SDL_MapRGBA(SDL_GetPixelFormatDetails(surface->format), nullptr, 0, 0, 0, 0));
		SDL_ClearSurface(surface.get(), 0.0f, 0, 0, 0);

		picker_height = m_tile_layer->tileset->num_tiles / picker_width;

		BoundingBox picker_bbox{ 0, 0, surface->w, surface->h };

		for (std::shared_ptr<rom::SpriteTile>& sprite_tile : tiles)
		{
			sprite_tile->BlitPixelDataToSurface(surface.get(), picker_bbox, sprite_tile->pixel_data);
		}

		texture = Renderer::RenderToTexture(surface.get());
	}

	void TilePicker::Draw()
	{
		if (spintool::DrawPaletteLineSelector(current_palette_line, m_tile_layer->palette_set, m_owning_ui))
		{
			RenderTileset();
		}
		if (texture != nullptr)
		{
			if (ImGui::BeginChild("TilePicker", ImVec2{ static_cast<float>(texture->w) * zoom + 16, -1 }))
			{

				const ImVec2 cursor_start_pos = ImGui::GetCursorScreenPos();
				ImGui::Image((ImTextureID)texture.get(),
					ImVec2{ static_cast<float>(texture->w) * zoom, static_cast<float>(texture->h) * zoom },
					ImVec2{ 0,0 }, ImVec2{ static_cast<float>(surface->w) / texture->w, static_cast<float>(surface->h) / texture->h });

				for (unsigned int grid_y = 0; grid_y < picker_height; ++grid_y)
				{
					for (unsigned int grid_x = 0; grid_x < picker_width; ++grid_x)
					{
						const unsigned int target_index = (grid_y * picker_width) + grid_x;

						if (target_index >= tiles.size())
						{
							continue;
						}
						const rom::SpriteTile& tile = *tiles[target_index];

						const ImVec2 min = ImVec2{ static_cast<float>(cursor_start_pos.x + (tile.x_offset * zoom)), static_cast<float>(cursor_start_pos.y + (tile.y_offset * zoom)) };
						const ImVec2 max = ImVec2{ static_cast<float>(min.x + (tile.x_size * zoom) + 1), static_cast<float>(min.y + (tile.y_size * zoom) + 1) };
						const bool is_hovered = ImGui::IsMouseHoveringRect(min, max);

						if (is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
						{
							currently_selected_tile = &tile;
						}

						if (is_hovered || &tile == currently_selected_tile)
						{
							const ImU32 colour = is_hovered ? ImGui::GetColorU32(ImVec4(255, 255, 0, 255)) : ImGui::GetColorU32(ImVec4(0, 255, 0, 255));

							ImGui::GetWindowDrawList()->AddRect(min, max, colour);
						}
					}
				}
			}
			else
			{
				ImGui::Text("<Failed to render tileset>");
			}
			ImGui::EndChild();
		}
	}

	void TilePicker::SetTileLayer(rom::TileLayer* layer)
	{
		const bool render_required = m_tile_layer != layer;
		m_tile_layer = layer;
		if (render_required)
		{
			RenderTileset();
		}
	}

	spintool::rom::TileLayer* TilePicker::GetTileLayer()
	{
		return m_tile_layer;
	}

}