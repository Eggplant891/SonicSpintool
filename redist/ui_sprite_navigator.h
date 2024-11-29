#pragma once

#include "ui_sprite.h"
#include "ui_palette_viewer.h"

#include <vector>

namespace spintool
{
	class EditorUI;
}

namespace spintool
{
	class EditorSpriteNavigator
	{
	public:
		EditorSpriteNavigator(EditorUI& owning_ui);

		void Update();
		void SetPalettes(std::vector<VDPPalette>& palettes);
		void InitialisePalettes(const size_t num_palettes);

	private:
		EditorUI& m_owning_ui;
		SpinballROM& m_rom;

		std::vector<UIPalette> m_palettes;
		std::vector<UISpriteTexture> m_sprites_found;

		EditorPaletteViewer m_palette_viewer;

		size_t m_starting_offset = 0;
		size_t m_selected_sprite_rom_offset = 0;
		int m_offset = 0;
		int m_chosen_palette = 0;
		bool m_use_packed_data_mode = true;
		bool m_read_between_sprites = false;
	};
}