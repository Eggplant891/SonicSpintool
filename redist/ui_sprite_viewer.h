#pragma once
#include "spinball_rom.h"

#include "ui_sprite.h"

#include <memory>
#include <vector>

namespace spintool
{
	class EditorSpriteViewer
	{
	public:
		EditorSpriteViewer(const std::shared_ptr<SpinballSprite>& sprite, const std::vector<UIPalette>& palettes);
		void Update();

		bool IsOpen() const { return m_is_open; }
		std::unique_ptr<UISpriteTextureAllPalettes> m_sprites;

	private:
		bool m_is_open = true;
	};
}