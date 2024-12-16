#pragma once

#include <vector>
#include "ui_palette.h"

namespace spintool
{
	class EditorUI;
}

namespace spintool
{
	struct PaletteWidgetSettings
	{
		bool show_name = true;
		bool show_palette_preview = true;
		bool allow_editing_palette_swatches = false;
	};

	struct PaletteWidgetResults
	{
		std::optional<int> new_palette_selection;
		std::optional<VDPPalette> modified_palette_swatches;
	};

	bool DrawPaletteSelectorWithPreview(int& palette_index, const EditorUI& owning_ui);
	void DrawPaletteName(const VDPPalette& palette, int palette_index);
	void DrawPaletteSwatchEditor(VDPPalette& palette, int palette_index);
	void DrawPaletteSwatchPreview(const VDPPalette& palette, int palette_index);
	bool DrawPaletteSelector(int& chosen_palette, const EditorUI& owning_ui);

	class EditorPaletteViewer
	{
	public:
		EditorPaletteViewer(EditorUI& owning_ui);

		void Update(std::vector<VDPPalette>& palettes);

		bool visible = false;

	private:
		EditorUI& m_owning_ui;
	};
}