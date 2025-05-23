#pragma once

#include "SDL3/SDL_pixels.h"
#include "types/sdl_handle_defs.h"
#include "types/bounding_box.h"
#include <mutex>

namespace spintool::rom
{
	class SpinballROM;

	struct Sprite;
	struct Palette;
	struct PaletteSet;
	struct SpriteTile;
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

		static SDLPaletteHandle CreateSDLPalette(const rom::Palette& palette);
		static SDLPaletteHandle CreateSDLPaletteForSet(const rom::PaletteSet& palette_set);
		static void SetPalette(const SDLPaletteHandle& palette);

		static SDLTextureHandle RenderToTexture(const rom::Sprite& sprite, bool flip_x = false, bool flip_y = false);
		static SDLTextureHandle RenderToTexture(const rom::SpriteTile& sprite_tile);
		static SDLTextureHandle RenderToTexture(SDL_Surface* surface);
		static SDLTextureHandle RenderArbitaryOffsetToTexture(const rom::SpinballROM& rom, Uint32 offset, Point dimensions);
		static SDLTextureHandle RenderArbitaryOffsetToTexture(const rom::SpinballROM& rom, Uint32 offset, Point dimensions, const rom::Palette& palette);
		static SDLTextureHandle RenderArbitaryOffsetToTilesetTexture(const rom::SpinballROM& rom, Uint32 offset, Point dimensions_in_tiles);

		static SDL_Texture* GetViewportTexture();

		static SDL_Renderer* s_renderer;
		static SDL_Window* s_window;
		static SDL_Surface* s_window_surface;
		static SDLTextureHandle s_window_texture;
		static SDLPaletteHandle s_current_palette;

		static std::recursive_mutex s_sdl_update_mutex;

		constexpr static const int s_window_width = 1600;
		constexpr static const int s_window_height = 900;
	};
}