#include "ui/ui_sprite_viewer.h"
#include "ui/ui_palette_viewer.h"
#include "ui/ui_editor.h"
#include <memory>

namespace spintool
{
	size_t EditorSpriteViewer::GetOffset() const
	{
		return m_offset;
	}

	EditorSpriteViewer::EditorSpriteViewer(EditorUI& owning_ui, std::shared_ptr<const rom::Sprite> spr)
		: EditorWindowBase(owning_ui)
		, m_sprite(spr)
		, m_rendered_sprite_texture(spr)
		, m_offset(spr->rom_data.rom_offset)
	{
	}

	void EditorSpriteViewer::Update()
	{
		if (m_sprite == nullptr)
		{
			return;
		}

		char name[64];
		sprintf_s(name, "Sprite Viewer (0x%06X)", static_cast<unsigned int>(m_sprite->rom_data.rom_offset));
		ImGui::SetNextWindowSize(ImVec2(static_cast<float>(m_sprite->GetBoundingBox().Width() * m_zoom) + 32, 640), ImGuiCond_Appearing);
		if (ImGui::Begin(name, &m_visible, ImGuiWindowFlags_NoSavedSettings))
		{
			ImGui::SliderFloat("Zoom", &m_zoom, 1.0f, 8.0f, "%.1f");
			ImGui::Text("0x%06X -> 0x%06X ", m_sprite->rom_data.rom_offset, m_sprite->rom_data.rom_offset_end - 1);

			const rom::Sprite& selected_sprite = *m_sprite;

			ImGui::Text("Num Tiles: %d (%02X)", selected_sprite.num_tiles, selected_sprite.num_tiles);
			ImGui::Text("Num VDP Tiles (8x8): %d (%02X)", selected_sprite.num_vdp_tiles, selected_sprite.num_vdp_tiles);
			ImGui::Text("Dimensions X: %d (0x%02X)", selected_sprite.GetBoundingBox().Width(), selected_sprite.GetBoundingBox().Width());
			ImGui::SameLine();
			ImGui::Text("Y: %d (0x%02X)", selected_sprite.GetBoundingBox().Height(), selected_sprite.GetBoundingBox().Height());
			ImGui::Text("Size on ROM: %d (0x%04X)", selected_sprite.rom_data.real_size, selected_sprite.rom_data.real_size);

			ImGui::Checkbox("Render Tile Borders", &m_render_tile_borders);
			ImGui::SameLine();
			ImGui::Checkbox("Render Origin", &m_render_sprite_origin);

			if (DrawPaletteSelector(m_chosen_palette_index, m_owning_ui))
			{
				m_rendered_sprite_texture.texture.reset();
				for (UISpriteTileTexture& tile_tex : m_rendered_sprite_tile_textures)
				{
					tile_tex.texture.reset();
				}
			}
			DrawPaletteSwatchPreview(m_selected_palette, m_chosen_palette_index);

			char name_buffer[128];
			int i = 0;

			if (m_rendered_sprite_texture.texture == nullptr)
			{
				m_selected_palette = *m_owning_ui.GetPalettes().at(m_chosen_palette_index);
				m_rendered_sprite_texture.texture = m_rendered_sprite_texture.RenderTextureForPalette(m_selected_palette);
			}

			if (ImGui::BeginChild("sprite_render_zone", ImVec2{ 0,0 }, ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
			{
				const ImVec2 cursor_pos = ImGui::GetCursorPos();
				ImGui::SetCursorPos(ImVec2{ cursor_pos.x + 1, cursor_pos.y + 1 });
				m_rendered_sprite_texture.DrawForImGui(m_zoom);

				const BoundingBox image_preview_pos = { static_cast<int>(ImGui::GetItemRectMin().x), static_cast<int>(ImGui::GetItemRectMin().y), static_cast<int>(ImGui::GetItemRectMax().x), static_cast<int>(ImGui::GetItemRectMax().y) };

				ImGui::SameLine();
				ImGui::Dummy(ImVec2{ 1,1 });
				const Point origin_point = m_sprite->GetOriginOffsetFromMinBounds();
				if (m_render_sprite_origin)
				{
					ImGui::GetWindowDrawList()->AddRect(
						{ image_preview_pos.min.x + ((origin_point.x - 1) * m_zoom), image_preview_pos.min.y + ((origin_point.y - 1) * m_zoom) },
						{ image_preview_pos.min.x + ((origin_point.x + 1) * m_zoom), image_preview_pos.min.y + ((origin_point.y + 1) * m_zoom) },
						ImGui::GetColorU32(ImVec4{ 64, 64, 64, 255 }));
				}

				if (m_render_tile_borders)
				{
					for (const std::shared_ptr<rom::SpriteTile>& sprite_tile : m_sprite->sprite_tiles)
					{
						Point tile_offset{ sprite_tile->x_offset + origin_point.x, sprite_tile->y_offset + origin_point.y };
						ImGui::GetWindowDrawList()->AddRect(
							{ image_preview_pos.min.x + (tile_offset.x * m_zoom) - 1, image_preview_pos.min.y + (tile_offset.y * m_zoom) - 1 },
							{ image_preview_pos.min.x + (tile_offset.x * m_zoom) + (sprite_tile->x_size * m_zoom) + 1, image_preview_pos.min.y + (tile_offset.y * m_zoom) + (sprite_tile->y_size * m_zoom) + 1 }
						, ImGui::GetColorU32(ImVec4{ 0,192,0,255 }), 0.0f, 0, 1);
					}
				}

				if (ImGui::TreeNode("Tiles"))
				{
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

						sprintf_s(name_buffer, "Tile %d", i);
						if (ImGui::TreeNode(name_buffer))
						{
							if (tile_tex->texture == nullptr)
							{
								tile_tex->texture = tile_tex->RenderTextureForPalette(m_selected_palette);
							}


							if (tile_tex->texture != nullptr)
							{
								tile_tex->DrawForImGui(m_zoom);

							}
							else
							{
								ImGui::BeginDisabled();
								ImGui::Text("Failed to render texture");
								ImGui::EndDisabled();
							}

							if (sprite_tile->header_rom_data.real_size != 0)
							{
								ImGui::Text("Header address range: 0x%06X -> 0x%06X", sprite_tile->header_rom_data.rom_offset, sprite_tile->header_rom_data.rom_offset_end - 1);
							}
							if (sprite_tile->tile_rom_data.real_size != 0)
							{
								ImGui::Text("Data address range: 0x%06X -> 0x%06X", sprite_tile->tile_rom_data.rom_offset, sprite_tile->tile_rom_data.rom_offset_end - 1);
							}

							ImGui::Text("Offset X: %d (0x%04X)", sprite_tile->x_offset, sprite_tile->x_offset);
							ImGui::Text("Offset Y: %d (0x%04X)", sprite_tile->y_offset, sprite_tile->y_offset);

							ImGui::Text("Dimensions X: %d (0x%02X)", sprite_tile->x_size, sprite_tile->x_size);
							ImGui::Text("Dimensions Y: %d (0x%02X)", sprite_tile->y_size, sprite_tile->y_size);
							int working_val[2] = { sprite_tile->x_size, sprite_tile->y_size };
							ImGui::Text("Pixels: %d (0x%04X)", sprite_tile->pixel_data.size(), sprite_tile->pixel_data.size());

							ImGui::TreePop();
						}
					}
					ImGui::TreePop();
				}
			}
			ImGui::EndChild();
		}
		ImGui::End();
	}
}