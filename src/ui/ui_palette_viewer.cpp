#include "ui/ui_palette_viewer.h"

#include "SDL3/SDL_stdinc.h"
#include "ui/ui_editor.h"
#include "render.h"


namespace spintool
{
	bool DrawPaletteSelectorWithPreview(int& palette_index, const EditorUI& owning_ui)
	{
		const bool selection_changed = DrawPaletteSelector(palette_index, owning_ui);
		ImGui::SameLine();
		const rom::Palette& palette = *owning_ui.GetPalettes().at(palette_index);
		DrawPaletteName(palette, palette_index);
		ImGui::SameLine();
		ImGui::Dummy(ImVec2{ 0,0 });
		DrawPaletteSwatchPreview(palette, palette_index);

		return selection_changed;
	}

	void DrawPaletteName(const rom::Palette& palette, int palette_index)
	{
		ImGui::Text("Palette %02X (0x%04X)", palette_index, palette.offset);
	}

	void DrawPaletteSwatchEditor(rom::Palette& palette, int palette_index)
	{
		ImGui::PushStyleVarX(ImGuiStyleVar_ItemSpacing, 0);
		for (rom::Swatch& swatch : palette.palette_swatches)
		{
			char swatch_name[64];
			sprintf_s(swatch_name, "0x%02X###swatch_%p", swatch.packed_value, &swatch);

			ImColor col = swatch.AsImColor();
			ImGui::SameLine();
			if (ImGui::ColorEdit3(swatch_name, &col.Value.x, ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoInputs))
			{
				swatch.packed_value = rom::Swatch::Pack(col.Value.x, col.Value.y, col.Value.z);
			}
		}
		ImGui::PopStyleVar();
	}

	void DrawPaletteSwatchPreview(const rom::Palette& palette, int palette_index)
	{
		ImGui::NewLine();
		ImGui::PushStyleVarX(ImGuiStyleVar_ItemSpacing, 0);
		for (const rom::Swatch& swatch : palette.palette_swatches)
		{
			char swatch_name[64];
			sprintf_s(swatch_name, "0x%02X###swatch_%p", swatch.packed_value, &swatch);

			ImColor col = swatch.AsImColor();
			ImGui::SameLine();
			ImGui::ColorButton(swatch_name, col.Value);
		}
		ImGui::PopStyleVar();
	}

	bool DrawPaletteSelector(int& chosen_palette, const EditorUI& owning_ui)
	{
		ImGui::SetNextItemWidth(256);
		const bool selection_changed = ImGui::SliderInt("###Palette Index", &chosen_palette, 0, static_cast<int>(owning_ui.GetPalettes().size() - 1));
		return selection_changed;
	}

	EditorPaletteViewer::EditorPaletteViewer(EditorUI& owning_ui)
		: m_owning_ui(owning_ui)
	{

	}

	void EditorPaletteViewer::Update(std::vector<std::shared_ptr<rom::Palette>>& palettes)
	{
		if (visible == false)
		{
			return;
		}

		if (ImGui::Begin("Palettes", &visible))
		{
			if (ImGui::Button("Save Changes"))
			{
				rom::SpinballROM& rom = m_owning_ui.GetROM();
				Uint8* current_byte = &rom.m_buffer[palettes.front()->offset];
				for (std::shared_ptr<rom::Palette>& palette : palettes)
				{
					for (rom::Swatch& swatch : palette->palette_swatches)
					{
						*current_byte = (swatch.packed_value & 0xFF00) >> 8;
						++current_byte;
						*current_byte = (swatch.packed_value & 0x00FF);
						++current_byte;
					}
				}
			}
			ImGui::Separator();
			if (ImGui::BeginChild("swatch_list"))
			{
				int palette_index = 0;
				for (std::shared_ptr<rom::Palette>& palette : palettes)
				{
					DrawPaletteName(*palette, palette_index);
					ImGui::SameLine();
					ImGui::Dummy(ImVec2{ 0,0 });
					DrawPaletteSwatchEditor(*palette, palette_index);
					++palette_index;
				}
			}
			ImGui::EndChild();
		}
		ImGui::End();
	}

}