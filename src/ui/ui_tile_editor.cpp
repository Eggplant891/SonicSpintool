#include "ui/ui_tile_editor.h"

#include "rom/level.h"
#include "imgui.h"
#include "render.h"
#include "ui/ui_palette_viewer.h"

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

		ImGui::SetNextWindowPos(ImVec2{ 0,16 });
		ImGui::SetNextWindowSize(ImVec2{ Renderer::s_window_width, Renderer::s_window_height - 16 });
		if (ImGui::Begin("Tile Editor", &m_visible, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings))
		{
			if (m_target_brush == nullptr)
			{
				ImGui::Text("<INVALID BRUSH TARGET>");
			}

			if (ImGui::Button("Save Tile"))
			{
				m_visible = false;
			}
			ImGui::SameLine();
			if (ImGui::Button("Close Window"))
			{
				m_visible = false;
			}
			
			m_tile_picker.Draw();
			ImGui::SameLine();

			const float max_zoom = 16.0f;
			float zoom = max_zoom;

			const ImVec2 tile_origin_screen = ImGui::GetCursorScreenPos();

			const ImVec2 cursor_start_pos = ImGui::GetCursorScreenPos();

			for (; zoom >= 1.0f; zoom = zoom / 2.0f)
			{
				ImGui::Image((ImTextureID)m_brush_preview.texture.get(), ImVec2{ 32 * zoom ,  32 * zoom }); ImGui::SameLine();
			}
			ImGui::NewLine();
			for (unsigned int grid_y = 0; grid_y < m_target_brush->BrushHeight(); ++grid_y)
			{
				for (unsigned int grid_x = 0; grid_x < m_target_brush->BrushWidth(); ++grid_x)
				{
					const unsigned int target_index = (grid_y * m_target_brush->BrushWidth()) + grid_x;

					if (target_index >= m_target_brush->tiles.size())
					{
						continue;
					}

					const ImVec2 min = ImVec2{ tile_origin_screen.x + (grid_x * 8 * max_zoom), tile_origin_screen.y + (grid_y * 8 * max_zoom) };
					const ImVec2 max = ImVec2{ tile_origin_screen.x + ((grid_x + 1) * 8 * max_zoom), tile_origin_screen.y + ((grid_y + 1) * 8 * max_zoom) };
					const bool is_hovered = ImGui::IsMouseHoveringRect(min, max);

					if (is_hovered)
					{
						if (m_tile_picker.currently_selected_tile != nullptr)
						{
							const auto& tile = *m_tile_picker.currently_selected_tile;

							ImGui::SetCursorScreenPos(min);
							ImGui::Image((ImTextureID)m_tile_picker.texture.get(),
								ImVec2{ (max.x - min.x), (max.y - min.y) },
								ImVec2{ static_cast<float>(tile.x_offset) / m_tile_picker.texture->w , static_cast<float>(tile.y_offset) / m_tile_picker.texture->h },
								ImVec2{ static_cast<float>(tile.x_offset + tile.x_size) / m_tile_picker.texture->w, static_cast<float>(tile.y_offset + tile.y_size) / m_tile_picker.texture->h });

							if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
							{
								m_target_brush->tiles[target_index].tile_index = static_cast<int>(std::distance(std::begin(m_tile_picker.tiles)
									, std::find_if(std::begin(m_tile_picker.tiles), std::end(m_tile_picker.tiles),
										[this](const std::shared_ptr<rom::SpriteTile>& tile)
										{
											return m_tile_picker.currently_selected_tile == tile.get();
										})));

								m_target_brush->tiles[target_index].palette_line = m_tile_picker.current_palette_line;
								m_tile_brush_changed = true;
							}
						}

						if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
						{
							m_tile_picker.currently_selected_tile = m_tile_picker.tiles[m_target_brush->tiles[target_index].tile_index].get();
						}

						if (ImGui::IsKeyPressed(ImGuiKey_R, false))
						{
							m_target_brush->tiles[target_index].is_flipped_horizontally = !m_target_brush->tiles[target_index].is_flipped_horizontally;
							m_tile_brush_changed = true;
						}

						if (ImGui::IsKeyPressed(ImGuiKey_F, false))
						{
							m_target_brush->tiles[target_index].is_flipped_vertically = !m_target_brush->tiles[target_index].is_flipped_vertically;
							m_tile_brush_changed = true;
						}

						if (ImGui::IsKeyPressed(ImGuiKey_H, false))
						{
							m_target_brush->tiles[target_index].is_high_priority = !m_target_brush->tiles[target_index].is_high_priority;
							m_tile_brush_changed = true;
						}
					}
					const bool is_high_priority = m_target_brush->tiles[target_index].is_high_priority;
					ImGuiCol grid_colour = is_high_priority == false ? ImGui::GetColorU32(ImVec4(255, 0, 0, 255)) : ImGui::GetColorU32(ImVec4(255, 0, 255, 255));
					if (is_hovered)
					{
						grid_colour = is_high_priority == false ? ImGui::GetColorU32(ImVec4(255, 255, 0, 255)) : ImGui::GetColorU32(ImVec4(255, 255, 255, 255));
					}
					ImGui::GetWindowDrawList()->AddRect(min, max, grid_colour);
				}
			}
		}
		ImGui::End();

		if (m_tile_brush_changed)
		{
			RenderBrush();
			m_tile_brush_changed = false;
		}
	}

	EditorTileEditor::EditorTileEditor(EditorUI& owning_ui, rom::TileLayer& tile_layer, Uint32 brush_index)
		: EditorWindowBase(owning_ui)
		, m_tile_layer(&tile_layer)
		, m_tile_picker(owning_ui)
		, m_brush_index(brush_index)
	{
		m_target_brush = m_tile_layer->tile_layout->tile_brushes[m_brush_index].get();
		m_tile_picker.SetTileLayer(m_tile_layer);
		RenderBrush();
	}
}
