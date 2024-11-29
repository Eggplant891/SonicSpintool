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

	void EditorPaletteViewer::RenderPalette(const UIPalette& palette, size_t palette_index)
	{
		ImGui::Text("Palette %d (0x%2X)", palette_index, palette.palette.offset);
		for (const VDPSwatch& swatch : palette.palette.palette_swatches)
		{
			char swatch_name[64];
			sprintf_s(swatch_name, "0x%2X###swatch_%p", swatch.packed_value, &swatch);

			float col[3] = { static_cast<float>(swatch.GetUnpacked().r / 255.0f), static_cast<float>(swatch.GetUnpacked().g / 255.0f), static_cast<float>(swatch.GetUnpacked().b / 255.0f) };
			ImGui::SameLine();
			ImGui::ColorEdit3(swatch_name, col, ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoInputs);
		}
	}

	void EditorPaletteViewer::Update(std::vector<UIPalette>& palettes)
	{
		size_t palette_index = 0;
		for (const UIPalette& palette : palettes)
		{
			++palette_index;
			RenderPalette(palette, palette_index);
		}
	}

}