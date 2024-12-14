#pragma once

#include "SDL3/SDL_pixels.h"
#include "sdl_handle_defs.h"
#include <mutex>

namespace spintool
{
	struct VDPPalette;
	struct SpinballSprite;
	struct SpriteTile;
	class SpinballROM;
}

namespace spintool
{
	class Renderer
	{
	public:
		static void Initialise();
		static void Shutdown();
		static void NewFrame();
		static void Render();

		static SDLPaletteHandle CreatePalette(const VDPPalette& palette);
		static void SetPalette(const SDLPaletteHandle& palette);

		static SDLTextureHandle RenderToTexture(const SpinballSprite& sprite);
		static SDLTextureHandle RenderToTexture(const SpriteTile& sprite_tile);
		static SDLTextureHandle RenderToTexture(SDL_Surface* surface);
		static SDLTextureHandle RenderArbitaryOffsetToTexture(const SpinballROM& rom, size_t offset, Point dimensions);
		static SDL_Texture* GetTexture();
		static SDL_Texture* GetViewportTexture();

		static SDL_PixelFormat s_pixel_format;
		static const SDL_PixelFormatDetails* s_pixel_format_details;
		static const SDL_PixelFormatDetails* s_bitmap_pixel_format_details;

		static std::recursive_mutex s_sdl_update_mutex;

		constexpr static const int window_width = 1600;
		constexpr static const int window_height = 900;
	};
}