#include "render.h"

#include <iostream>

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
	SDLPaletteHandle Renderer::s_current_palette;
	std::recursive_mutex Renderer::s_sdl_update_mutex;

	void Renderer::SetPalette(const SDLPaletteHandle& palette)
	{
		if (!palette)
		{
			std::cerr << "Renderer::SetPalette received a null palette\n";
			return;
		}

		if (!s_current_palette || palette->ncolors != s_current_palette->ncolors)
		{
			s_current_palette = SDLPaletteHandle{ SDL_CreatePalette(palette->ncolors) };
			if (!s_current_palette)
			{
				std::cerr << "SDL_CreatePalette failed: " << SDL_GetError() << '\n';
				return;
			}
		}

		if (!SDL_SetPaletteColors(s_current_palette.get(), palette->colors, 0, palette->ncolors))
		{
			std::cerr << "SDL_SetPaletteColors failed: " << SDL_GetError() << '\n';
		}
	}

	SDLPaletteHandle Renderer::CreateSDLPalette(const rom::Palette& palette)
	{
		SDLPaletteHandle new_palette{ SDL_CreatePalette(16) };
		if (!new_palette)
		{
			std::cerr << "SDL_CreatePalette failed: " << SDL_GetError() << '\n';
			return {};
		}

		for (size_t i = 0; i < 16; ++i)
		{
			rom::Colour colour = palette.palette_swatches[i].GetUnpacked();
			new_palette->colors[i] = { colour.r, colour.g, colour.b, 255 };
		}
		return new_palette;
	}

	SDLPaletteHandle Renderer::CreateSDLPaletteForSet(const rom::PaletteSet& palette_set)
	{
		SDLPaletteHandle new_palette{ SDL_CreatePalette(64) };
		if (!new_palette)
		{
			std::cerr << "SDL_CreatePalette failed: " << SDL_GetError() << '\n';
			return {};
		}

		for (size_t a = 0; a < 4; ++a)
		{
			if (!palette_set.palette_lines[a])
			{
				std::cerr << "Palette set contains a null palette line\n";
				return {};
			}

			for (size_t i = 0; i < 16; ++i)
			{
				rom::Colour colour = palette_set.palette_lines[a]->palette_swatches[i].GetUnpacked();
				new_palette->colors[(a * 16) + i] = { colour.r, colour.g, colour.b, 255 };
			}
		}
		return new_palette;
	}

	bool Renderer::Initialise()
	{
		if (!SDL_CreateWindowAndRenderer(
			"SpinTool",
			s_window_width,
			s_window_height,
			SDL_WINDOW_RESIZABLE,
			&s_window,
			&s_renderer))
		{
			std::cerr << "SDL_CreateWindowAndRenderer failed: " << SDL_GetError() << '\n';
			return false;
		}

		if (!s_window || !s_renderer)
		{
			std::cerr << "SDL returned a null window or renderer\n";
			Shutdown();
			return false;
		}

		if (!SDL_MaximizeWindow(s_window))
		{
			std::cerr << "SDL_MaximizeWindow failed: " << SDL_GetError() << '\n';
		}

		if (!SDL_SetRenderDrawColor(s_renderer, 0, 0, 0, 255) ||
			!SDL_RenderClear(s_renderer))
		{
			std::cerr << "SDL renderer setup failed: " << SDL_GetError() << '\n';
			Shutdown();
			return false;
		}

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();

		// The configurable font scale starts at 100%.
		ImGuiIO& io = ImGui::GetIO();
		io.Fonts->AddFontDefault();
		io.FontGlobalScale = 1.0f;

		if (!ImGui_ImplSDL3_InitForSDLRenderer(s_window, s_renderer))
		{
			std::cerr << "ImGui SDL3 backend initialisation failed\n";
			Shutdown();
			return false;
		}

		if (!ImGui_ImplSDLRenderer3_Init(s_renderer))
		{
			std::cerr << "ImGui SDL renderer backend initialisation failed\n";
			ImGui_ImplSDL3_Shutdown();
			Shutdown();
			return false;
		}

		return true;
	}

	void Renderer::Shutdown()
	{
		if (ImGui::GetCurrentContext())
		{
			ImGui_ImplSDLRenderer3_Shutdown();
			ImGui_ImplSDL3_Shutdown();
			ImGui::DestroyContext();
		}

		if (s_renderer)
		{
			SDL_DestroyRenderer(s_renderer);
			s_renderer = nullptr;
		}

		if (s_window)
		{
			SDL_DestroyWindow(s_window);
			s_window = nullptr;
		}
	}

	void Renderer::NewFrame()
	{
		if (!s_renderer || !s_window || !ImGui::GetCurrentContext())
		{
			return;
		}

		ImGui_ImplSDLRenderer3_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();
	}

	void Renderer::Render()
	{
		if (!s_renderer || !ImGui::GetCurrentContext())
		{
			return;
		}

		if (!SDL_RenderClear(s_renderer))
		{
			std::cerr << "SDL_RenderClear failed: " << SDL_GetError() << '\n';
			return;
		}

		ImGui::Render();
		ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), s_renderer);
		SDL_RenderPresent(s_renderer);
	}

	SDLTextureHandle Renderer::RenderArbitaryOffsetToTilesetTexture(const rom::SpinballROM& rom, Uint32 offset, Point dimensions_in_tiles)
	{
		const Point tile_dimensions{ rom::TileSet::s_tile_width, rom::TileSet::s_tile_height, };
		const Point dimensions{ dimensions_in_tiles.x * tile_dimensions.x, dimensions_in_tiles.y * tile_dimensions.y };
		SDLSurfaceHandle new_surface{ SDL_CreateSurface(dimensions.x, dimensions.y, SDL_PIXELFORMAT_RGBA32) };
		SDLSurfaceHandle tile_surface{ SDL_CreateSurface(rom::TileSet::s_tile_width, rom::TileSet::s_tile_height, SDL_PIXELFORMAT_RGBA32) };
		if (!new_surface || !tile_surface)
		{
			std::cerr << "SDL_CreateSurface failed: " << SDL_GetError() << '\n';
			return {};
		}
		Uint32 next_offset = offset;
		for (int i = 0; i < dimensions_in_tiles.x * dimensions_in_tiles.y; ++i)
		{
			SDL_ClearSurface(tile_surface.get(), 0.0f, 0.0f, 0.0f, 0.0f);
			rom.RenderToSurface(tile_surface.get(), offset, tile_dimensions);

			SDL_Rect target_rect{ (i % dimensions_in_tiles.x) * tile_surface->w, ((i - (i % dimensions_in_tiles.x)) / dimensions_in_tiles.x) * tile_surface->h, tile_surface->w, tile_surface->h};

			if (!SDL_BlitSurface(tile_surface.get(), nullptr, new_surface.get(), &target_rect))
			{
				std::cerr << "SDL_BlitSurface failed: " << SDL_GetError() << '\n';
				return {};
			}
			offset += static_cast<Uint32>(tile_surface->w * tile_surface->h * 2);
		}
		return RenderToTexture(new_surface.get());
	}

	SDLTextureHandle Renderer::RenderArbitaryOffsetToTexture(const rom::SpinballROM& rom, Uint32 offset, Point dimensions)
	{
		SDLSurfaceHandle new_surface{ SDL_CreateSurface(dimensions.x, dimensions.y, SDL_PIXELFORMAT_RGBA32) };
		if (!new_surface)
		{
			std::cerr << "SDL_CreateSurface failed: " << SDL_GetError() << '\n';
			return {};
		}
		rom.RenderToSurface(new_surface.get(), offset, dimensions);
		return RenderToTexture(new_surface.get());
	}

	SDLTextureHandle Renderer::RenderArbitaryOffsetToTexture(const rom::SpinballROM& rom, Uint32 offset, Point dimensions, const rom::Palette& palette)
	{
		SDLSurfaceHandle new_surface{ SDL_CreateSurface(dimensions.x, dimensions.y, SDL_PIXELFORMAT_RGBA32) };
		if (!new_surface)
		{
			std::cerr << "SDL_CreateSurface failed: " << SDL_GetError() << '\n';
			return {};
		}
		//UIPalette ui_palette{palette};
		//SDL_SetSurfacePalette(new_surface.get(), ui_palette.sdl_palette.get());
		rom.RenderToSurface(new_surface.get(), offset, dimensions, palette);
		return RenderToTexture(new_surface.get());
	}

	SDLTextureHandle Renderer::RenderToTexture(const rom::Sprite& sprite, bool flip_x, bool flip_y)
	{
		if (!s_current_palette)
		{
			std::cerr << "Cannot render sprite: no palette has been selected\n";
			return {};
		}

		SDLSurfaceHandle sprite_atlas_surface{ SDL_CreateSurface(
			sprite.GetBoundingBox().Width(),
			sprite.GetBoundingBox().Height(),
			SDL_PIXELFORMAT_INDEX8) };

		if (!sprite_atlas_surface)
		{
			std::cerr << "SDL_CreateSurface failed: " << SDL_GetError() << '\n';
			return {};
		}

		if (!SDL_SetSurfacePalette(sprite_atlas_surface.get(), s_current_palette.get()) ||
			!SDL_SetSurfaceColorKey(sprite_atlas_surface.get(), true, 0))
		{
			std::cerr << "Sprite surface setup failed: " << SDL_GetError() << '\n';
			return {};
		}

		sprite.RenderToSurface(sprite_atlas_surface.get());
		if (flip_x && !SDL_FlipSurface(sprite_atlas_surface.get(), SDL_FLIP_HORIZONTAL))
		{
			std::cerr << "SDL_FlipSurface failed: " << SDL_GetError() << '\n';
			return {};
		}

		if (flip_y && !SDL_FlipSurface(sprite_atlas_surface.get(), SDL_FLIP_VERTICAL))
		{
			std::cerr << "SDL_FlipSurface failed: " << SDL_GetError() << '\n';
			return {};
		}

		return RenderToTexture(sprite_atlas_surface.get());
	}

	SDLTextureHandle Renderer::RenderToTexture(const rom::SpriteTile& sprite_tile)
	{
		if (!s_current_palette)
		{
			std::cerr << "Cannot render sprite tile: no palette has been selected\n";
			return {};
		}

		SDLSurfaceHandle sprite_atlas_surface{
			SDL_CreateSurface(sprite_tile.x_size, sprite_tile.y_size, SDL_PIXELFORMAT_INDEX8)
		};
		if (!sprite_atlas_surface)
		{
			std::cerr << "SDL_CreateSurface failed: " << SDL_GetError() << '\n';
			return {};
		}

		if (!SDL_SetSurfacePalette(sprite_atlas_surface.get(), s_current_palette.get()))
		{
			std::cerr << "SDL_SetSurfacePalette failed: " << SDL_GetError() << '\n';
			return {};
		}

		sprite_tile.RenderToSurface(sprite_atlas_surface.get());
		return RenderToTexture(sprite_atlas_surface.get());
	}

	SDLTextureHandle Renderer::RenderToTexture(SDL_Surface* surface)
	{
		if (!s_renderer || !surface)
		{
			std::cerr << "Cannot create texture: null renderer or surface\n";
			return {};
		}

		SDLTextureHandle new_texture{ SDL_CreateTextureFromSurface(s_renderer, surface) };
		if (!new_texture)
		{
			std::cerr << "SDL_CreateTextureFromSurface failed: " << SDL_GetError() << '\n';
			return {};
		}

		if (!SDL_SetTextureScaleMode(new_texture.get(), SDL_SCALEMODE_NEAREST))
		{
			std::cerr << "SDL_SetTextureScaleMode failed: " << SDL_GetError() << '\n';
			return {};
		}
		return new_texture;
	}

}
