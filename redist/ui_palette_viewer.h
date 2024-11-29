#pragma once

#include <vector>
#include "ui_palette.h"

namespace spintool
{
	class EditorUI;
}

namespace spintool
{
	class EditorPaletteViewer
	{
	public:
		EditorPaletteViewer(EditorUI& owning_ui);
		static void RenderPalette(const UIPalette& palette, size_t palette_index = 0);
		void Update(std::vector<UIPalette>& palettes);

	private:
		EditorUI& m_owning_ui;
	};
}