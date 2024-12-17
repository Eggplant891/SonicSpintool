#pragma once

#include "types/sdl_handle_defs.h"
#include "rom/spinball_rom.h"
#include "imgui.h"
#include "ui/ui_palette.h"

#include <vector>

namespace spintool
{
	struct UISpriteTileTexture
	{
		UISpriteTileTexture(const std::shared_ptr<rom::SpriteTile>& sprite_tile);
		std::shared_ptr<const rom::SpriteTile> sprite_tile;
		mutable SDLTextureHandle texture;
		ImVec2 dimensions;

		SDLTextureHandle RenderTextureForPalette(const UIPalette& palette) const;
	};

	struct UISpriteTexture
	{
		UISpriteTexture(std::shared_ptr<const rom::Sprite>& spr);
		std::shared_ptr<const rom::Sprite> sprite;
		SDLTextureHandle texture;
		ImVec2 dimensions;

		std::vector<UISpriteTileTexture> tile_textures;

		SDLTextureHandle RenderTextureForPalette(const UIPalette& palette) const;
	};
}