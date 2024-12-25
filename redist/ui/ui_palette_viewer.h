#pragma once

#include "ui/ui_palette.h"
#include "ui/ui_editor_window.h"

#include <vector>

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
		std::optional<rom::Palette> modified_palette_swatches;
	};

	bool DrawPaletteSelectorWithPreview(int& palette_index, const EditorUI& owning_ui);
	void DrawPaletteName(const rom::Palette& palette, int palette_index);
	void DrawPaletteSwatchEditor(rom::Palette& palette, int palette_index);
	void DrawPaletteSwatchPreview(const rom::Palette& palette, int palette_index);
	bool DrawPaletteSelector(int& chosen_palette, const EditorUI& owning_ui);

	class EditorPaletteViewer : public EditorWindowBase
	{
	public:
		using EditorWindowBase::EditorWindowBase;
		void Update() override;
	};
}