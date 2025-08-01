#include "ui/ui_tile_picker.h"

#include "render.h"
#include "rom/tile.h"
#include "rom/level.h"
#include "rom/sprite_tile.h"
#include "rom/tileset.h"

#include "imgui.h"
#include "ui/ui_palette_viewer.h"
#include "ui/ui_file_selector.h"
#include "ui/ui_editor.h"
#include "SDL3/SDL_image.h"

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
		m_surface.reset();
		m_texture.reset();
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

		m_surface = SDLSurfaceHandle{ SDL_CreateSurface(max_x_size, max_y_size, SDL_PIXELFORMAT_RGBA32) };
		SDL_SetSurfaceColorKey(m_surface.get(), true, SDL_MapRGBA(SDL_GetPixelFormatDetails(m_surface->format), nullptr, 0, 0, 0, 0));
		SDL_ClearSurface(m_surface.get(), 0.0f, 0, 0, 0);

		picker_height = (m_tile_layer->tileset->num_tiles / picker_width) + 1;

		BoundingBox picker_bbox{ 0, 0, m_surface->w, m_surface->h };

		for (std::shared_ptr<rom::SpriteTile>& sprite_tile : tiles)
		{
			sprite_tile->BlitPixelDataToSurface(m_surface.get(), picker_bbox, sprite_tile->pixel_data);
		}

		m_texture = Renderer::RenderToTexture(m_surface.get());
	}

	void TilePicker::Draw()
	{
		if (spintool::DrawPaletteLineSelector(current_palette_line, m_tile_layer->palette_set))
		{
			RenderTileset();
		}
		if (m_texture != nullptr)
		{
			if (ImGui::BeginChild("TilePicker", ImVec2{ static_cast<float>(m_texture->w) * zoom + 16, -1 }))
			{

				const ImVec2 cursor_start_pos = ImGui::GetCursorScreenPos();
				ImGui::Image((ImTextureID)m_texture.get(),
					ImVec2{ static_cast<float>(m_texture->w) * zoom, static_cast<float>(m_texture->h) * zoom },
					ImVec2{ 0,0 }, ImVec2{ static_cast<float>(m_surface->w) / m_texture->w, static_cast<float>(m_surface->h) / m_texture->h });

				for (unsigned int grid_y = 0; grid_y < picker_height; ++grid_y)
				{
					for (unsigned int grid_x = 0; grid_x < picker_width; ++grid_x)
					{
						const unsigned int target_index = (grid_y * picker_width) + grid_x;

						if (target_index >= tiles.size() || target_index >= m_tile_layer->tileset->tiles.size())
						{
							continue;
						}
						const rom::SpriteTile& tile = *tiles[target_index];

						const ImVec2 min = ImVec2{ static_cast<float>(cursor_start_pos.x + (tile.x_offset * zoom)), static_cast<float>(cursor_start_pos.y + (tile.y_offset * zoom)) };
						const ImVec2 max = ImVec2{ static_cast<float>(min.x + (tile.x_size * zoom) + 1), static_cast<float>(min.y + (tile.y_size * zoom) + 1) };
						const bool is_hovered = ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(min, max);

						if (is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
						{
							currently_selected_tile = &tile;
						}

						if (is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
						{
							ImGui::OpenPopup("TilesetOptions");
						}

						if (is_hovered || &tile == currently_selected_tile)
						{
							const ImU32 colour = is_hovered ? ImGui::GetColorU32(ImVec4(255, 255, 0, 255)) : ImGui::GetColorU32(ImVec4(0, 255, 0, 255));

							ImGui::GetWindowDrawList()->AddRect(min, max, colour);
						}
					}
				}

				static FileSelectorSettings settings;
				settings.object_typename = "Image";
				settings.target_directory = m_owning_ui.GetSpriteExportPath();
				settings.file_extension_filter = { ".png", ".gif", ".bmp" };
				settings.tiled_previews = true;
				settings.num_columns = 4;

				if (ImGui::BeginPopup("TilesetOptions"))
				{
					if (ImGui::Selectable("Import Image"))
					{
						m_owning_ui.OpenImageImporter(const_cast<rom::TileSet&>(*m_tile_layer->tileset), m_tile_layer->palette_set);
					}

					if (ImGui::Selectable("Export Tileset"))
					{
						static char path_buffer[2048];
						sprintf_s(path_buffer, "spinball_tileset_%X02.png", static_cast<unsigned int>(m_tile_layer->tileset->rom_data.rom_offset));
						std::filesystem::path export_path = m_owning_ui.GetSpriteExportPath().append(path_buffer);
						SDLSurfaceHandle out_surface = m_tile_layer->tileset->RenderToSurface(*m_tile_layer->palette_set.palette_lines.at(current_palette_line));
						assert(IMG_SavePNG(out_surface.get(), export_path.generic_u8string().c_str()));
					}
					ImGui::EndPopup();
				}
			}
			else
			{
				ImGui::Text("<Failed to render tileset>");
			}
			ImGui::EndChild();
		}
	}

	void TilePicker::SetPaletteLine(int palette_line)
	{
		if (palette_line < 4)
		{
			current_palette_line = palette_line;
			RenderTileset();
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

	size_t TilePicker::GetSelectedTileIndex() const
	{
		return std::distance(std::begin(tiles)
			, std::find_if(std::begin(tiles), std::end(tiles),
				[this](const std::shared_ptr<rom::SpriteTile>& tile)
				{
					return currently_selected_tile == tile.get();
				}));
	}

	const rom::Tile* TilePicker::GetSelectedTile() const
	{
		if (currently_selected_tile == nullptr)
		{
			return nullptr;
		}

		return &m_tile_layer->tileset->tiles.at(GetSelectedTileIndex());
	}

	void TilePicker::DrawPickedTile(bool flip_x, bool flip_y, float zoom) const
	{
		if (currently_selected_tile == nullptr)
		{
			return;
		}

		const rom::SpriteTile& tile = *currently_selected_tile;

		ImVec2 uv0 = { static_cast<float>(tile.x_offset) / m_texture->w , static_cast<float>(tile.y_offset) / m_texture->h };
		ImVec2 uv1 = { static_cast<float>(tile.x_offset + tile.x_size) / m_texture->w, static_cast<float>(tile.y_offset + tile.y_size) / m_texture->h };
		if (flip_x)
		{
			std::swap(uv0.x, uv1.x);
		}
		if (flip_y)
		{
			std::swap(uv0.y, uv1.y);
		}

		ImGui::Image((ImTextureID)m_texture.get(), ImVec2{ static_cast<float>(tile.x_size), static_cast<float>(tile.y_size) } * zoom, uv0, uv1);
	}

}