#pragma once

#include "SDL3/SDL_pixels.h"
#include "sdl_handle_defs.h"
#include <mutex>

namespace spintool
{
	struct VDPPalette;
	struct SpinballSprite;
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

		static void RenderSprite(const SpinballSprite& sprite);
		static SDLTextureHandle RenderToTexture(const SpinballSprite& sprite);
		static SDL_Texture* GetTexture();

		static SDL_PixelFormat s_pixel_format;
		static const SDL_PixelFormatDetails* s_pixel_format_details;

		static std::recursive_mutex s_sdl_update_mutex;
	};
}