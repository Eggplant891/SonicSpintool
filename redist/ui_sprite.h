#pragma once

#include "sdl_handle_defs.h"
#include "spinball_rom.h"
#include "imgui.h"
#include "ui_palette.h"

#include <vector>

namespace spintool
{
	struct UISpriteTileTexture
	{
		UISpriteTileTexture(const std::shared_ptr<SpriteTile>& sprite_tile);
		std::shared_ptr<const SpriteTile> sprite_tile;
		mutable SDLTextureHandle texture;
		ImVec2 dimensions;

		SDLTextureHandle RenderTextureForPalette(const UIPalette& palette) const;
	};

	struct UISpriteTexture
	{
		UISpriteTexture(std::shared_ptr<const SpinballSprite>& spr);
		std::shared_ptr<const SpinballSprite> sprite;
		SDLTextureHandle texture;
		ImVec2 dimensions;

		std::vector<UISpriteTileTexture> tile_textures;

		SDLTextureHandle RenderTextureForPalette(const UIPalette& palette) const;
	};
}