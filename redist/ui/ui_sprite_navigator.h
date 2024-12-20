#pragma once

#include "ui/ui_sprite.h"
#include "ui/ui_palette_viewer.h"

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

		bool visible = false;

	private:
		EditorUI& m_owning_ui;
		rom::SpinballROM& m_rom;

		std::vector<std::shared_ptr<UISpriteTexture>> m_sprites_found;
		SDLTextureHandle m_random_texture;
		int random_tex_width = 128;
		int random_tex_height = 128;

		size_t m_starting_offset = 0;
		size_t m_selected_sprite_rom_offset = 0;
		size_t m_offset = 0x14D2;
		int m_chosen_palette = 0;
		float m_zoom = 2.0f;

		bool m_render_arbitrary_with_palette = true;
		bool m_attempt_render_of_arbitrary_data = false;
	};
}