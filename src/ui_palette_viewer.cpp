#include "ui_palette_viewer.h"

#include "SDL3/SDL_stdinc.h"
#include "ui_editor.h"
#include "render.h"


namespace spintool
{
	bool DrawPaletteSelectorWithPreview(int& palette_index, const EditorUI& owning_ui)
	{
		const bool selection_changed = DrawPaletteSelector(palette_index, owning_ui);
		ImGui::SameLine();
		const VDPPalette& palette = owning_ui.GetPalettes().at(palette_index);
		DrawPaletteName(palette, palette_index);
		ImGui::SameLine();
		ImGui::Dummy(ImVec2{ 0,0 });
		DrawPalettePreview(const_cast<VDPPalette&>(palette), palette_index);

		return selection_changed;
	}

	void DrawPaletteName(const VDPPalette& palette, int palette_index)
	{
		ImGui::Text("Palette %02X (0x%04X)", palette_index, palette.offset);
	}

	void DrawPalettePreview(VDPPalette& palette, int palette_index)
	{
		ImGui::PushStyleVarX(ImGuiStyleVar_ItemSpacing, 0);
		for (VDPSwatch& swatch : palette.palette_swatches)
		{
			char swatch_name[64];
			sprintf_s(swatch_name, "0x%02X###swatch_%p", swatch.packed_value, &swatch);

			ImColor col = swatch.AsImColor();
			ImGui::SameLine();
			if (ImGui::ColorEdit3(swatch_name, &col.Value.x, ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoInputs))
			{
				swatch.packed_value = VDPSwatch::Pack(col.Value.x, col.Value.y, col.Value.z);
			}
		}
		ImGui::PopStyleVar();
	}

	bool DrawPaletteSelector(int& chosen_palette, const EditorUI& owning_ui)
	{
		ImGui::SetNextItemWidth(256);
		const bool selection_changed = ImGui::SliderInt("###Palette Index", &chosen_palette, 0, static_cast<int>(owning_ui.GetPalettes().size() - 1));
		VDPPalette& palette = const_cast<VDPPalette&>(owning_ui.GetPalettes().at(chosen_palette));
		return selection_changed;
	}

	EditorPaletteViewer::EditorPaletteViewer(EditorUI& owning_ui)
		: m_owning_ui(owning_ui)
	{

	}

	void EditorPaletteViewer::Update(std::vector<VDPPalette>& palettes)
	{
		if (visible == false)
		{
			return;
		}

		if (ImGui::Begin("Palettes", &visible))
		{
			int palette_index = 0;
			for (VDPPalette& palette : palettes)
			{
				DrawPaletteName(palette, palette_index);
				ImGui::SameLine();
				ImGui::Dummy(ImVec2{ 0,0 });
				DrawPalettePreview(palette, palette_index);
				++palette_index;
			}
		}
		ImGui::End();
	}

}