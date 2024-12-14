#include "ui_sprite_viewer.h"

namespace spintool
{
	EditorSpriteViewer::EditorSpriteViewer(const std::shared_ptr<SpinballSprite>& spr, const std::vector<UIPalette>& palettes)
		: m_sprites(std::make_unique<UISpriteTextureAllPalettes>(spr, palettes))
	{

	}

	void EditorSpriteViewer::Update()
	{
		if (m_sprites == nullptr || m_sprites->sprite == nullptr)
		{
			return;
		}

		char name[64];
		sprintf_s(name, "Sprite Viewer(%02X)", static_cast<unsigned int>(m_sprites->sprite->rom_offset));
		ImGui::SetNextWindowSize(ImVec2(320, 640), ImGuiCond_Appearing);
		if (ImGui::Begin(name, &m_is_open, ImGuiWindowFlags_NoSavedSettings))
		{
			const int max_width = static_cast<int>(ImGui::GetWindowWidth());
			int current_width = 0;
			bool started_newline = true;
			ImGui::Text("0x%2X", m_sprites->sprite->rom_offset);
			ImGui::NewLine();
			if (ImGui::TreeNode("Sprite palettes"))
			{
				for (UISpriteTexture& tex : m_sprites->textures)
				{
					bool started_newline = false;
					if (tex.texture == nullptr || tex.dimensions.x == 0 || tex.dimensions.y == 0)
					{
						continue;
					}

					if (current_width + tex.dimensions.x > max_width)
					{
						started_newline = true;
						ImGui::NewLine();
						current_width = 0;
						ImGui::SameLine(); ImGui::Dummy(ImVec2(0, 0));
					}

					if (started_newline == false)
					{
						ImGui::SameLine();
					}

					ImGui::Image((ImTextureID)tex.texture.get(), ImVec2(static_cast<float>(tex.dimensions.x), static_cast<float>(tex.dimensions.y)), { 0,0 }, { static_cast<float>(tex.dimensions.x) / tex.texture->w, static_cast<float>((tex.dimensions.y) / tex.texture->h) });
					current_width += static_cast<int>(tex.dimensions.x + ImGui::GetStyle().ItemSpacing.x);
				}
				ImGui::TreePop();
			}
			const SpinballSprite& selected_sprite = *m_sprites->sprite;

			ImGui::Text("Num Tiles: %d (%02X)", selected_sprite.num_tiles, selected_sprite.num_tiles);
			ImGui::Text("Num VDP Tiles (8x8): %d (%02X)", selected_sprite.num_vdp_tiles, selected_sprite.num_vdp_tiles);

			size_t total_pixels = 0;
			char name_buffer[128];
			int i = 0;

			float zoom = 4.0f;

			UISpriteTexture& tex = m_sprites->textures.at(2);

			ImGui::Image((ImTextureID)tex.texture.get()
				, ImVec2(static_cast<float>(tex.dimensions.x) * zoom, static_cast<float>(tex.dimensions.y) * zoom)
				, { 0,0 }, { static_cast<float>(tex.dimensions.x) / tex.texture->w
				, static_cast<float>((tex.dimensions.y) / tex.texture->h) });


			for (const UISpriteTileTexture& tile_tex : m_sprites->textures.at(2).tile_textures)
			{
				++i;

				ImGui::Image((ImTextureID)tile_tex.texture.get()
					, ImVec2(static_cast<float>(tile_tex.dimensions.x) * zoom, static_cast<float>(tile_tex.dimensions.y) * zoom)
					, { 0,0 }
					, { static_cast<float>(tile_tex.dimensions.x) / tile_tex.texture->w, static_cast<float>((tile_tex.dimensions.y) / tile_tex.texture->h) });

				const SpriteTile& sprite_tile = *tile_tex.sprite_tile;
				sprintf_s(name_buffer, "Tile %d (%02X -> %02X )", i, static_cast<unsigned int>(tile_tex.sprite_tile->rom_offset), static_cast<unsigned int>(tile_tex.sprite_tile->rom_offset_end));
				if (ImGui::TreeNode(name_buffer))
				{
					ImGui::Text("Offset: X: %d (%02X) Y: %d (%02X)", sprite_tile.x_offset, sprite_tile.x_offset, sprite_tile.y_offset, sprite_tile.y_offset);

					ImGui::Text("X Size: %d", sprite_tile.x_size);
					ImGui::SameLine();
					ImGui::Text("Y Size: %d", sprite_tile.y_size);
					int working_val[2] = { sprite_tile.x_size, sprite_tile.y_size };
					ImGui::Text("Pixels: %d (%d)", sprite_tile.pixel_data.size(), sprite_tile.x_size * sprite_tile.y_size);

					ImGui::TreePop();
				}
				total_pixels += sprite_tile.pixel_data.size();
			}
			ImGui::Text("Total X Size: %d", selected_sprite.GetBoundingBox().Width());
			ImGui::Text("Total Y Size: %d", selected_sprite.GetBoundingBox().Height());
			ImGui::Text("Total Pixels: %d", static_cast<int>(total_pixels));
			ImGui::Text("Real Size: %04X", selected_sprite.real_size);
			ImGui::Text("Unrealised Size: %04X", selected_sprite.unrealised_size);
		}
		ImGui::End();
	}
}