#include "render.h"

#define SDL_ENABLE_OLD_NAMES

#include "SDL3/SDL.h"
#include "imgui.h"
#include "backends/imgui_impl_sdlrenderer3.h"
#include "backends/imgui_impl_sdl3.h"
#include "SDL3/SDL_oldnames.h"
#include "spinball_rom.h"
#include "SDL3/SDL_image.h"

namespace spintool
{
	constexpr int canvas_width = 128;
	constexpr int canvas_height = 128;

	static Point s_next_location = { 0,0 };

	SDL_Renderer* renderer;
	SDL_Window* window;
	SDLTextureHandle current_texture;
	SDLPaletteHandle current_palette;
	SDL_Surface* window_surface = nullptr;
	SDL_Surface* sprite_atlas_surface = nullptr;
	SDL_Surface* bitmap_sprite_surface = nullptr;

	SDL_PixelFormat Renderer::s_pixel_format;
	const SDL_PixelFormatDetails* Renderer::s_pixel_format_details = nullptr;
	const SDL_PixelFormatDetails* Renderer::s_bitmap_pixel_format_details = nullptr;
	std::recursive_mutex Renderer::s_sdl_update_mutex;

	SDLTextureHandle window_texture;

	SDL_Palette* palette;

	static Uint8 colour_levels[17] =
	{
		  0,
		 29,
		 52,
		 70,
		 87,
		101,
		116,
		130,
		144,
		158,
		172,
		187,
		206,
		228,
		255
	};

	VDPColour VDPSwatch::GetUnpacked() const
	{
		return VDPColour{ 0, colour_levels[(0x0F00 & packed_value) >> 8], colour_levels[(0x00F0 & packed_value) >> 4], colour_levels[(0x000F & packed_value)] };
	}


	Uint16 VDPSwatch::Pack(float r, float g, float b)
	{
		Uint8 red = static_cast<Uint8>(b * 15) << 8;
		Uint8 green = static_cast<Uint8>(b * 15) << 8;
		Uint8 blue = static_cast<Uint8>(b * 15) << 8;

		return static_cast<Uint16>(0x0F00 & colour_levels[blue]) | static_cast<Uint16>(0x00F0 & colour_levels[green]) | static_cast<Uint16>(0x000F & colour_levels[red]);
	}

	SDL_Texture* Renderer::GetTexture()
	{
		current_texture = SDLTextureHandle{ SDL_CreateTextureFromSurface(renderer, sprite_atlas_surface) };
		SDL_SetTextureScaleMode(current_texture.get(), SDL_ScaleMode::SDL_SCALEMODE_NEAREST);
		return current_texture.get();
	}

	SDL_Texture* Renderer::GetViewportTexture()
	{
		return window_texture.get();
	}

	void Renderer::SetPalette(const SDLPaletteHandle& palette)
	{
		SDL_SetSurfacePalette(sprite_atlas_surface, palette.get());
	}

	SDLPaletteHandle Renderer::CreateSDLPalette(const VDPPalette& palette)
	{
		SDLPaletteHandle new_palette{ SDL_CreatePalette(16) };
		for (size_t i = 0; i < 16; ++i)
		{
			VDPColour colour= palette.palette_swatches[i].GetUnpacked();
			new_palette->colors[i] = { colour.r, colour.g, colour.b, 255 };
		}
		return new_palette;
	}

	void Renderer::Initialise()
	{
		SDL_CreateWindowAndRenderer("Sonic Spintool", window_width, window_height, 0, &window, &renderer);
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
		SDL_RenderClear(renderer);
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

		ImGui::CreateContext();
		ImGui::StyleColorsDark();
		ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
		ImGui_ImplSDLRenderer3_Init(renderer);

		sprite_atlas_surface = SDL_CreateSurface(canvas_width, canvas_height, SDL_PIXELFORMAT_INDEX8);
		bitmap_sprite_surface = SDL_CreateSurface(canvas_width, canvas_height, SDL_PIXELFORMAT_RGBA32);
		window_surface = SDL_CreateSurface(window_width, window_height, SDL_PIXELFORMAT_RGBA32);
		s_pixel_format = sprite_atlas_surface->format;
		s_pixel_format_details = SDL_GetPixelFormatDetails(sprite_atlas_surface->format);
		s_bitmap_pixel_format_details = SDL_GetPixelFormatDetails(bitmap_sprite_surface->format);


		SDL_ClearSurface(window_surface, 0, 0, 255, 255);
		window_texture = SDLTextureHandle{ SDL_CreateTextureFromSurface(renderer, window_surface) };
	}

	void Renderer::Shutdown()
	{
		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(window);
	}


	void Renderer::NewFrame()
	{
		ImGui_ImplSDLRenderer3_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();

		window_texture = SDLTextureHandle{ SDL_CreateTextureFromSurface(renderer, window_surface) };
	}

	void Renderer::Render()
	{
		SDL_RenderClear(renderer);

		//SDL_BlitSurfaceScaled(sprite_atlas_surface, NULL, window_surface, NULL, SDL_ScaleMode::SDL_SCALEMODE_NEAREST);
		//SDL_SetTextureScaleMode(current_texture.get(), SDL_ScaleMode::SDL_SCALEMODE_NEAREST);
		//SDL_RenderTexture(renderer, current_texture.get(), NULL, NULL);

		SDL_RenderTexture(renderer, window_texture.get(), nullptr, nullptr);
		ImGui::Render();
		ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
		SDL_RenderPresent(renderer);
	}

	SDLTextureHandle Renderer::RenderArbitaryOffsetToTexture(const SpinballROM& rom, size_t offset, Point dimensions)
	{
		rom.RenderToSurface(bitmap_sprite_surface, offset, dimensions);
		return RenderToTexture(bitmap_sprite_surface);
	}

	SDLTextureHandle Renderer::RenderToTexture(const SpinballSprite& sprite)
	{
		sprite.RenderToSurface(sprite_atlas_surface);
		return RenderToTexture(sprite_atlas_surface);
	}

	SDLTextureHandle Renderer::RenderToTexture(const SpriteTile& sprite_tile)
	{
		sprite_tile.RenderToSurface(sprite_atlas_surface);
		return RenderToTexture(sprite_atlas_surface);
	}

	SDLTextureHandle Renderer::RenderToTexture(SDL_Surface* surface)
	{
		SDL_LockSurface(surface);
		SDLTextureHandle new_texture = SDLTextureHandle{ SDL_CreateTextureFromSurface(renderer, surface) };
		SDL_SetTextureScaleMode(new_texture.get(), SDL_ScaleMode::SDL_SCALEMODE_NEAREST);
		SDL_UnlockSurface(surface);
		return new_texture;
	}

}