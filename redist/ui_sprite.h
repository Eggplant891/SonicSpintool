#pragma once

#include "sdl_handle_defs.h"
#include "spinball_rom.h"
#include "imgui.h"
#include "ui_palette.h"

#include <vector>

namespace spintool
{
	struct UISpriteTexture
	{
		UISpriteTexture(const SpinballSprite& sprite);
		SpinballSprite sprite;
		SDLTextureHandle texture;
		ImVec2 dimensions;

		SDLTextureHandle RenderTextureForPalette(const UIPalette& palette) const;
	};

	struct UISpriteTextureAllPalettes
	{
		UISpriteTextureAllPalettes(const SpinballSprite& sprite, const std::vector<UIPalette>& palettes);
		SpinballSprite sprite;
		std::vector<UISpriteTexture> textures;
	};
}