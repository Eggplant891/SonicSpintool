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
		const std::shared_ptr<SpriteTile> sprite_tile;
		mutable SDLTextureHandle texture;
		ImVec2 dimensions;

		SDLTextureHandle RenderTextureForPalette(const UIPalette& palette) const;
	};

	struct UISpriteTexture
	{
		UISpriteTexture(const std::shared_ptr<SpinballSprite>& spr);
		const std::shared_ptr<SpinballSprite> sprite;
		SDLTextureHandle texture;
		ImVec2 dimensions;

		std::vector<UISpriteTileTexture> tile_textures;

		SDLTextureHandle RenderTextureForPalette(const UIPalette& palette) const;
	};

	struct UISpriteTextureAllPalettes
	{
		UISpriteTextureAllPalettes(const std::shared_ptr<SpinballSprite>& spr, const std::vector<UIPalette>& palettes);
		const std::shared_ptr<SpinballSprite>& sprite;
		std::vector<UISpriteTexture> textures;
	};
}