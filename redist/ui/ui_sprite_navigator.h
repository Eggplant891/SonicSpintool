#pragma once

#include "ui/ui_editor_window.h"
#include "ui/ui_sprite.h"
#include "ui/ui_palette_viewer.h"

#include <vector>


namespace spintool
{
	class EditorUI;
}

namespace spintool
{
	class EditorSpriteNavigator : public EditorWindowBase
	{
	public:
		using EditorWindowBase::EditorWindowBase;

		void Update() override;

	private:
		std::vector<std::shared_ptr<UISpriteTexture>> m_sprites_found;
		SDLTextureHandle m_random_texture;
		int m_arbitrary_num_tiles_width = 16;
		int m_arbitrary_num_tiles_height = 16;
		int m_arbitrary_texture_width = 128;
		int m_arbitrary_texture_height = 128;

		Uint32 m_starting_offset = 0;
		Uint32 m_selected_sprite_rom_offset = 0;
		Uint32 m_offset = 0x14D2;
		int m_chosen_palette = 0;
		float m_zoom = 2.0f;

		enum class ArbitraryRenderMode
		{
			VDP_COLOUR,
			VDP_TILES,
			PALETTE,
		};

		ArbitraryRenderMode m_render_arbitrary_with_palette = ArbitraryRenderMode::PALETTE;
		bool m_attempt_render_of_arbitrary_data = false;
		bool m_show_arbitrary_render = false;
	};
}