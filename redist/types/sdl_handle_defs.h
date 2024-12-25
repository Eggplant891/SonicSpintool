#pragma once

#include <memory>
#include "SDL3/SDL_render.h"

struct SDLTextureDeleter
{
	void operator()(SDL_Texture* tex) const
	{
		SDL_DestroyTexture(tex);
	}
};
using SDLTextureHandle = std::unique_ptr<SDL_Texture, SDLTextureDeleter>;

struct SDLSurfaceDeleter
{
	void operator()(SDL_Surface* surface) const
	{
		SDL_DestroySurface(surface);
	}
};
using SDLSurfaceHandle = std::unique_ptr<SDL_Surface, SDLSurfaceDeleter>;

struct SDLPaletteDeleter
{
	void operator()(SDL_Palette* palette) const
	{
		SDL_DestroyPalette(palette);
	}
};
using SDLPaletteHandle = std::unique_ptr<SDL_Palette, SDLPaletteDeleter>;

