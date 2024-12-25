#pragma once

#include "rom/spinball_rom.h"
#include "ui/ui_sprite.h"
#include "ui_editor_window.h"

#include <memory>
#include <vector>

namespace spintool
{
	class EditorUI;
}

namespace spintool
{
	class EditorSpriteViewer : public EditorWindowBase
	{
	public:
		EditorSpriteViewer(EditorUI& owning_ui, std::shared_ptr<const rom::Sprite> sprite);
		void Update();
		size_t GetOffset() const;

	private:
		size_t m_offset;
		std::shared_ptr<const rom::Sprite> m_sprite;
		UISpriteTexture m_rendered_sprite_texture;
		std::vector<UISpriteTileTexture> m_rendered_sprite_tile_textures;
		rom::Palette m_selected_palette;
		int m_chosen_palette_index = 0;
		float m_zoom = 4.0f;
		bool m_render_tile_borders = true;
		bool m_render_sprite_origin = true;
	};
}