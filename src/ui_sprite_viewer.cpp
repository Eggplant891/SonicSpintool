#include "ui_sprite_viewer.h"

namespace spintool
{
	EditorSpriteViewer::EditorSpriteViewer(const SpinballSprite& spr, const std::vector<UIPalette>& palettes)
		: m_sprites(std::make_unique<UISpriteTextureAllPalettes>(spr, palettes))
	{

	}

	void EditorSpriteViewer::Update()
	{
		char name[64];
		sprintf_s(name, "Sprite Viewer(%02X)", static_cast<unsigned int>(m_sprites->sprite.rom_offset));
		if (ImGui::Begin(name, &m_is_open, ImGuiWindowFlags_NoSavedSettings))
		{
			const int max_width = static_cast<int>(ImGui::GetWindowWidth());
			int current_width = 0;
			bool started_newline = true;
			ImGui::Text("0x%2X", m_sprites->sprite.rom_offset);
			ImGui::NewLine();
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

			SpinballSprite& selected_sprite = m_sprites->sprite;

			ImGui::Text("Offset: X: %d (%02X) Y: %d (%02X)", selected_sprite.x_offset, selected_sprite.x_offset, selected_sprite.y_offset, selected_sprite.y_offset);

			ImGui::Text("X Size: %d", selected_sprite.x_size);
			ImGui::SameLine();
			ImGui::Text("Y Size: %d", selected_sprite.y_size);
			int working_val[2] = { selected_sprite.x_size, selected_sprite.y_size };
			if (ImGui::InputInt2("Change Size", working_val, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue))
			{
				selected_sprite.x_size = static_cast<Uint8>(working_val[0]);
				selected_sprite.y_size = static_cast<Uint8>(working_val[1]);
				//RenderForPalettesAndEmplace(selected_sprite);
			}
			ImGui::Text("Total Pixels: %d (%d)", selected_sprite.pixel_data.size(), selected_sprite.x_size * selected_sprite.y_size);
			ImGui::Text("Real Size: %04X", selected_sprite.real_size);
			ImGui::Text("Unrealised Size: %04X", selected_sprite.unrealised_size);
		}
		ImGui::End();
	}
}