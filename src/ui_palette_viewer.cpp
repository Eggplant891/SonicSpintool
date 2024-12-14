#include "ui_palette_viewer.h"

#include "ui_editor.h"

#include "render.h"
#include "SDL3/SDL_stdinc.h"


namespace spintool
{

	EditorPaletteViewer::EditorPaletteViewer(EditorUI& owning_ui)
		: m_owning_ui(owning_ui)
	{

	}

	void EditorPaletteViewer::RenderPalette(UIPalette& palette, size_t palette_index)
	{
		ImGui::Text("Palette %d (0x%2X)", palette_index, palette.palette.offset);
		for (VDPSwatch& swatch : palette.palette.palette_swatches)
		{
			char swatch_name[64];
			sprintf_s(swatch_name, "0x%2X###swatch_%p", swatch.packed_value, &swatch);

			ImColor col = swatch.AsImColor();
			ImGui::SameLine();
			if (ImGui::ColorEdit3(swatch_name, &col.Value.x, ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoInputs))
			{
				swatch.packed_value = VDPSwatch::Pack(col.Value.x, col.Value.y, col.Value.z);
				palette = UIPalette(palette.palette);
			}
		}
	}

	void EditorPaletteViewer::Update(std::vector<UIPalette>& palettes)
	{
		if (visible == false)
		{
			return;
		}

		if (ImGui::Begin("Palettes", &visible))
		{
			size_t palette_index = 0;
			for (UIPalette& palette : palettes)
			{
				RenderPalette(palette, palette_index);
				++palette_index;
			}
		}
		ImGui::End();
	}

}