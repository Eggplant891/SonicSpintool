#include "ui_sprite_viewer.h"
#include "ui_palette_viewer.h"
#include "ui_editor.h"

namespace spintool
{
	bool EditorSpriteViewer::IsOpen() const
	{
		return m_is_open;
	}

	size_t EditorSpriteViewer::GetOffset() const
	{
		return m_offset;
	}

	EditorSpriteViewer::EditorSpriteViewer(EditorUI& owning_ui, std::shared_ptr<const SpinballSprite> spr)
		: m_owning_ui(owning_ui)
		, m_sprite(spr)
		, m_rendered_sprite_texture(spr)
		, m_offset(spr->rom_offset)
	{
	}

	void EditorSpriteViewer::Update()
	{
		if (m_sprite == nullptr)
		{
			return;
		}

		char name[64];
		sprintf_s(name, "Sprite Viewer(%02X)", static_cast<unsigned int>(m_sprite->rom_offset));
		ImGui::SetNextWindowSize(ImVec2(320, 640), ImGuiCond_Appearing);
		if (ImGui::Begin(name, &m_is_open, ImGuiWindowFlags_NoSavedSettings))
		{
			ImGui::SliderFloat("Zoom", &m_zoom, 1.0f, 8.0f, "%.1f");
			ImGui::Text("0x%2X", m_sprite->rom_offset);

			const SpinballSprite& selected_sprite = *m_sprite;

			ImGui::Text("Num Tiles: %d (%02X)", selected_sprite.num_tiles, selected_sprite.num_tiles);
			ImGui::Text("Num VDP Tiles (8x8): %d (%02X)", selected_sprite.num_vdp_tiles, selected_sprite.num_vdp_tiles);

			if (DrawPaletteSelector(m_chosen_palette_index, m_owning_ui))
			{
				m_rendered_sprite_texture.texture.reset();
				for (UISpriteTileTexture& tile_tex : m_rendered_sprite_tile_textures)
				{
					tile_tex.texture.reset();
				}
			}


			size_t total_pixels = 0;
			char name_buffer[128];
			int i = 0;

			if (m_rendered_sprite_texture.texture == nullptr)
			{
				m_selected_palette = m_owning_ui.GetPalettes().at(m_chosen_palette_index);
				m_rendered_sprite_texture.texture = m_rendered_sprite_texture.RenderTextureForPalette(m_selected_palette);
			}

			ImGui::Image((ImTextureID)m_rendered_sprite_texture.texture.get()
				, ImVec2(static_cast<float>(m_rendered_sprite_texture.dimensions.x) * m_zoom, static_cast<float>(m_rendered_sprite_texture.dimensions.y) * m_zoom)
				, { 0,0 }, { static_cast<float>(m_rendered_sprite_texture.dimensions.x) / m_rendered_sprite_texture.texture->w
				, static_cast<float>((m_rendered_sprite_texture.dimensions.y) / m_rendered_sprite_texture.texture->h) });
			

			for (const std::shared_ptr<SpriteTile>& sprite_tile : m_sprite->sprite_tiles)
			{
				++i;

				auto ui_tile_tex = std::find_if(std::begin(m_rendered_sprite_tile_textures), std::end(m_rendered_sprite_tile_textures),
					[&sprite_tile](const UISpriteTileTexture& tile_tex)
					{
						return tile_tex.sprite_tile.get() == sprite_tile.get();
					});

				UISpriteTileTexture* tile_tex = nullptr;

				if (ui_tile_tex != std::end(m_rendered_sprite_tile_textures))
				{
					tile_tex = &*ui_tile_tex;
				}
				else
				{
					tile_tex = &m_rendered_sprite_tile_textures.emplace_back(sprite_tile);
				}

				if (tile_tex->texture == nullptr)
				{
					tile_tex->texture = tile_tex->RenderTextureForPalette(m_selected_palette);
				}

				sprintf_s(name_buffer, "Tile %d (%02X -> %02X )", i, static_cast<unsigned int>(sprite_tile->rom_offset), static_cast<unsigned int>(sprite_tile->rom_offset_end));
				if (ImGui::TreeNode(name_buffer))
				{
					if (tile_tex->texture != nullptr)
					{
						ImGui::Image((ImTextureID)tile_tex->texture.get()
							, ImVec2(static_cast<float>(tile_tex->dimensions.x) * m_zoom, static_cast<float>(tile_tex->dimensions.y) * m_zoom)
							, { 0,0 }
						, { static_cast<float>(tile_tex->dimensions.x) / tile_tex->texture->w, static_cast<float>((tile_tex->dimensions.y) / tile_tex->texture->h) });
					}
					else
					{
						ImGui::BeginDisabled();
						ImGui::Text("Failed to render texture");
						ImGui::EndDisabled();
					}
					ImGui::Text("Offset: X: %d (%02X) Y: %d (%02X)", sprite_tile->x_offset, sprite_tile->x_offset, sprite_tile->y_offset, sprite_tile->y_offset);

					ImGui::Text("X Size: %d", sprite_tile->x_size);
					ImGui::Text("Y Size: %d", sprite_tile->y_size);
					int working_val[2] = { sprite_tile->x_size, sprite_tile->y_size };
					ImGui::Text("Pixels: %d (%d)", sprite_tile->pixel_data.size(), sprite_tile->x_size * sprite_tile->y_size);

					ImGui::TreePop();
				}
				total_pixels += sprite_tile->pixel_data.size();
			}
			ImGui::Text("Total X Size: %d", selected_sprite.GetBoundingBox().Width());
			ImGui::Text("Total Y Size: %d", selected_sprite.GetBoundingBox().Height());
			ImGui::Text("Total Pixels: %d", static_cast<int>(total_pixels));
			ImGui::Text("Bytes: %04X", selected_sprite.real_size);
		}
		ImGui::End();
	}
}