#include "render.h"


#include "rom/spinball_rom.h"
#include "rom/sprite.h"
#include "rom/palette.h"

#define SDL_ENABLE_OLD_NAMES
#include "SDL3/SDL.h"
#include "SDL3/SDL_image.h"
#include "SDL3/SDL_oldnames.h"

#include "imgui.h"
#include "backends/imgui_impl_sdlrenderer3.h"
#include "backends/imgui_impl_sdl3.h"
#include "ui/ui_palette.h"

namespace spintool
{
	SDL_Renderer* Renderer::s_renderer = nullptr;
	SDL_Window* Renderer::s_window = nullptr;
	SDL_Surface* Renderer::s_window_surface = nullptr;
	SDLTextureHandle Renderer::s_window_texture;
	SDLPaletteHandle Renderer::s_current_palette;
	std::recursive_mutex Renderer::s_sdl_update_mutex;


	SDL_Texture* Renderer::GetViewportTexture()
	{
		return s_window_texture.get();
	}

	void Renderer::SetPalette(const SDLPaletteHandle& palette)
	{
		if (s_current_palette == nullptr || palette->ncolors != s_current_palette->ncolors)
		{
			s_current_palette = SDLPaletteHandle{ SDL_CreatePalette(palette->ncolors) };
		}
		*s_current_palette = *palette;
	}

	SDLPaletteHandle Renderer::CreateSDLPalette(const rom::Palette& palette)
	{
		SDLPaletteHandle new_palette{ SDL_CreatePalette(16) };
		for (size_t i = 0; i < 16; ++i)
		{
			rom::Colour colour= palette.palette_swatches[i].GetUnpacked();
			new_palette->colors[i] = { colour.r, colour.g, colour.b, 255 };
		}
		return new_palette;
	}

	SDLPaletteHandle Renderer::CreateSDLPaletteForSet(const rom::PaletteSet& palette_set)
	{
		SDLPaletteHandle new_palette{ SDL_CreatePalette(64) };
		for (size_t a = 0; a < 4; ++a)
		{
			for (size_t i = 0; i < 16; ++i)
			{
				rom::Colour colour = palette_set.palette_lines[a]->palette_swatches[i].GetUnpacked();
				new_palette->colors[(a * 16) + i] = { colour.r, colour.g, colour.b, 255 };
			}
		}
		return new_palette;
	}

	void Renderer::Initialise()
	{
		SDL_CreateWindowAndRenderer("Sonic Spintool", s_window_width, s_window_height, 0, &s_window, &s_renderer);
		SDL_SetRenderDrawColor(s_renderer, 0, 0, 0, 0);
		SDL_RenderClear(s_renderer);
		SDL_SetRenderDrawColor(s_renderer, 0, 0, 0, 255);

		ImGui::CreateContext();
		ImGui::StyleColorsDark();
		ImGui_ImplSDL3_InitForSDLRenderer(s_window, s_renderer);
		ImGui_ImplSDLRenderer3_Init(s_renderer);

		s_window_surface = SDL_CreateSurface(s_window_width, s_window_height, SDL_PIXELFORMAT_RGBA32);
		SDL_ClearSurface(s_window_surface, 0, 0, 255, 255);
		s_window_texture = SDLTextureHandle{ SDL_CreateTextureFromSurface(s_renderer, s_window_surface) };
	}

	void Renderer::Shutdown()
	{
		SDL_DestroyRenderer(s_renderer);
		SDL_DestroyWindow(s_window);
	}


	void Renderer::NewFrame()
	{
		ImGui_ImplSDLRenderer3_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();

		s_window_texture = SDLTextureHandle{ SDL_CreateTextureFromSurface(s_renderer, s_window_surface) };
	}

	void Renderer::Render()
	{
		SDL_RenderClear(s_renderer);

		SDL_RenderTexture(s_renderer, s_window_texture.get(), nullptr, nullptr);
		ImGui::Render();
		ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), s_renderer);
		SDL_RenderPresent(s_renderer);
	}

	SDLTextureHandle Renderer::RenderArbitaryOffsetToTilesetTexture(const rom::SpinballROM& rom, Uint32 offset, Point dimensions_in_tiles)
	{
		const Point tile_dimensions{ rom::TileSet::s_tile_width, rom::TileSet::s_tile_height, };
		const Point dimensions{ dimensions_in_tiles.x * tile_dimensions.x, dimensions_in_tiles.y * tile_dimensions.y };
		SDLSurfaceHandle new_surface{ SDL_CreateSurface( dimensions.x, dimensions.y, SDL_PIXELFORMAT_RGBA32) };

		SDLSurfaceHandle tile_surface{ SDL_CreateSurface(rom::TileSet::s_tile_width, rom::TileSet::s_tile_height, SDL_PIXELFORMAT_RGBA32) };
		Uint32 next_offset = offset;
		for (int i = 0; i < dimensions_in_tiles.x * dimensions_in_tiles.y; ++i)
		{
			SDL_ClearSurface(tile_surface.get(), 0.0f, 0.0f, 0.0f, 0.0f);
			rom.RenderToSurface(tile_surface.get(), offset, tile_dimensions);

			SDL_Rect target_rect{ (i % dimensions_in_tiles.x) * tile_surface->w, ((i - (i % dimensions_in_tiles.x)) / dimensions_in_tiles.x) * tile_surface->h, tile_surface->w, tile_surface->h};

			SDL_BlitSurface(tile_surface.get(), nullptr, new_surface.get(), &target_rect);
			offset += (tile_surface->w, tile_surface->h) * 2;
		}
		return RenderToTexture(new_surface.get());
	}

	SDLTextureHandle Renderer::RenderArbitaryOffsetToTexture(const rom::SpinballROM& rom, Uint32 offset, Point dimensions)
	{
		SDLSurfaceHandle new_surface{ SDL_CreateSurface(dimensions.x, dimensions.y, SDL_PIXELFORMAT_RGBA32) };
		rom.RenderToSurface(new_surface.get(), offset, dimensions);
		return RenderToTexture(new_surface.get());
	}

	SDLTextureHandle Renderer::RenderArbitaryOffsetToTexture(const rom::SpinballROM& rom, Uint32 offset, Point dimensions, const rom::Palette& palette)
	{
		SDLSurfaceHandle new_surface{ SDL_CreateSurface(dimensions.x, dimensions.y, SDL_PIXELFORMAT_RGBA32) };
		//UIPalette ui_palette{palette};
		//SDL_SetSurfacePalette(new_surface.get(), ui_palette.sdl_palette.get());
		rom.RenderToSurface(new_surface.get(), offset, dimensions, palette);
		return RenderToTexture(new_surface.get());
	}

	SDLTextureHandle Renderer::RenderToTexture(const rom::Sprite& sprite, bool flip_x, bool flip_y)
	{
		auto sprite_atlas_surface = SDL_CreateSurface(sprite.GetBoundingBox().Width(), sprite.GetBoundingBox().Height(), SDL_PIXELFORMAT_INDEX8);
		SDL_SetSurfacePalette(sprite_atlas_surface, s_current_palette.get());
		sprite.RenderToSurface(sprite_atlas_surface);
		if (flip_x)
		{
			SDL_FlipSurface(sprite_atlas_surface, SDL_FLIP_HORIZONTAL);
		}

		if (flip_y)
		{
			SDL_FlipSurface(sprite_atlas_surface, SDL_FLIP_VERTICAL);
		}

		return RenderToTexture(sprite_atlas_surface);
	}

	SDLTextureHandle Renderer::RenderToTexture(const rom::SpriteTile& sprite_tile)
	{
		auto sprite_atlas_surface = SDL_CreateSurface(sprite_tile.x_size, sprite_tile.y_size, SDL_PIXELFORMAT_INDEX8);
		SDL_SetSurfacePalette(sprite_atlas_surface, s_current_palette.get());
		sprite_tile.RenderToSurface(sprite_atlas_surface);
		return RenderToTexture(sprite_atlas_surface);
	}

	SDLTextureHandle Renderer::RenderToTexture(SDL_Surface* surface)
	{
		SDLTextureHandle new_texture = SDLTextureHandle{ SDL_CreateTextureFromSurface(s_renderer, surface) };
		SDL_SetTextureScaleMode(new_texture.get(), SDL_ScaleMode::SDL_SCALEMODE_NEAREST);
		return new_texture;
	}

}