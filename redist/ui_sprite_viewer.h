#pragma once
#include "spinball_rom.h"

#include "ui_sprite.h"

#include <memory>
#include <vector>

namespace spintool { class EditorUI; }

namespace spintool
{
	class EditorSpriteViewer
	{
	public:
		EditorSpriteViewer(EditorUI& owning_ui, std::shared_ptr<const SpinballSprite> sprite);
		void Update();
		bool IsOpen() const;
		size_t GetOffset() const;

	private:
		size_t m_offset;
		EditorUI& m_owning_ui;
		std::shared_ptr<const SpinballSprite> m_sprite;
		UISpriteTexture m_rendered_sprite_texture;
		std::vector<UISpriteTileTexture> m_rendered_sprite_tile_textures;
		VDPPalette m_selected_palette;
		int m_chosen_palette_index = 0;
		float m_zoom = 4.0f;;
		bool m_is_open = true;
	};
}