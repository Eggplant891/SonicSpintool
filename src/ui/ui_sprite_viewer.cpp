#include "ui/ui_sprite_viewer.h"
#include "ui/ui_palette_viewer.h"
#include "ui/ui_editor.h"
#include <memory>

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

	EditorSpriteViewer::EditorSpriteViewer(EditorUI& owning_ui, std::shared_ptr<const rom::Sprite> spr)
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

			const rom::Sprite& selected_sprite = *m_sprite;

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
			DrawPaletteSwatchPreview(m_selected_palette, m_chosen_palette_index);

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

			const BoundingBox image_preview_pos = { static_cast<int>(ImGui::GetItemRectMin().x), static_cast<int>(ImGui::GetItemRectMin().y), static_cast<int>(ImGui::GetItemRectMax().x), static_cast<int>(ImGui::GetItemRectMax().y) };

			const Point origin_point = m_sprite->GetOriginOffsetFromMinBounds();
			ImGui::GetWindowDrawList()->AddRect({ image_preview_pos.min.x + ((origin_point.x - 1) * m_zoom), image_preview_pos.min.y + ((origin_point.y - 1) * m_zoom) }, { image_preview_pos.min.x + ((origin_point.x + 1) * m_zoom), image_preview_pos.min.y + ((origin_point.y + 1) * m_zoom) }, ImGui::GetColorU32(ImVec4{64, 64, 64, 255}));

			for (const std::shared_ptr<rom::SpriteTile>& sprite_tile : m_sprite->sprite_tiles)
			{
				Point tile_offset{ sprite_tile->x_offset + origin_point.x, sprite_tile->y_offset + origin_point.y };
				ImGui::GetWindowDrawList()->AddRect(
					{ image_preview_pos.min.x + (tile_offset.x * m_zoom), image_preview_pos.min.y + (tile_offset.y * m_zoom) },
					{ image_preview_pos.min.x + (tile_offset.x * m_zoom) + (sprite_tile->x_size * m_zoom), image_preview_pos.min.y + (tile_offset.y * m_zoom) + (sprite_tile->y_size * m_zoom) }
				, ImGui::GetColorU32(ImVec4{ 0,192,0,255 }), 0.0f, 0, 1);
			}
			

			for (const std::shared_ptr<rom::SpriteTile>& sprite_tile : m_sprite->sprite_tiles)
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
					ImGui::Text("Offset X: %d (%02X)", sprite_tile->x_offset, sprite_tile->x_offset);
					ImGui::Text("Offset Y: %d (%02X)",  sprite_tile->y_offset, sprite_tile->y_offset);

					ImGui::Text("Dimensions X: %d", sprite_tile->x_size);
					ImGui::Text("Dimensions Y: %d", sprite_tile->y_size);
					int working_val[2] = { sprite_tile->x_size, sprite_tile->y_size };
					ImGui::Text("Pixels: %d (%d)", sprite_tile->pixel_data.size(), sprite_tile->x_size * sprite_tile->y_size);

					ImGui::TreePop();
				}
				total_pixels += sprite_tile->pixel_data.size();
			}
			ImGui::Text("Dimensions X: %d", selected_sprite.GetBoundingBox().Width());
			ImGui::Text("Dimensions Y: %d", selected_sprite.GetBoundingBox().Height());
			ImGui::Text("Total Pixels: %d", static_cast<int>(total_pixels));
			ImGui::Text("Size in ROM: %04X", selected_sprite.real_size);
		}
		ImGui::End();
	}
}