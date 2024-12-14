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
		void SetPalettes(const std::vector<VDPPalette>& palettes);
		std::vector<UIPalette>& GetPalettes();
		void InitialisePalettes(const size_t num_palettes);

		bool visible = false;

	private:
		EditorUI& m_owning_ui;
		SpinballROM& m_rom;

		std::vector<UIPalette> m_palettes;
		std::vector<std::shared_ptr<UISpriteTexture>> m_sprites_found;
		SDLTextureHandle m_random_texture;
		int random_tex_width = 128;
		int random_tex_height = 128;

		size_t m_starting_offset = 0;
		size_t m_selected_sprite_rom_offset = 0;
		size_t m_offset = 0x14D2;
		int m_chosen_palette = 0;
		bool m_use_packed_data_mode = true;
		bool m_read_between_sprites = false;
	};
}