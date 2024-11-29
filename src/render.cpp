#include "render.h"

#define SDL_ENABLE_OLD_NAMES

#include "SDL3/SDL.h"
#include "imgui.h"
#include "backends/imgui_impl_sdlrenderer3.h"
#include "backends/imgui_impl_sdl3.h"
#include "SDL3/SDL_oldnames.h"
#include "spinball_rom.h"

namespace spintool
{
	constexpr int window_width = 1600;
	constexpr int window_height = 900;

	constexpr int canvas_width = 320;
	constexpr int canvas_height = 240;

	SDL_Renderer* renderer;
	SDL_Window* window;
	SDLTextureHandle current_texture;
	SDLPaletteHandle current_palette;
	SDL_Surface* window_surface = nullptr;
	SDL_Surface* sprite_surface = nullptr;

	SDL_PixelFormat Renderer::s_pixel_format;
	const SDL_PixelFormatDetails* Renderer::s_pixel_format_details = nullptr;
	std::recursive_mutex Renderer::s_sdl_update_mutex;

	SDL_Palette* palette;

	spintool::VDPColour VDPSwatch::GetUnpacked() const
	{
		return VDPColour{ 0, static_cast<Uint8>(((0x0F00 & packed_value) >> 8) / 15.0f * 255), static_cast<Uint8>(((0x00F0 & packed_value) >> 4) / 15.0f * 255), static_cast<Uint8>(((0x000F & packed_value)) / 15.0f * 255) };
	}

	SDL_Texture* Renderer::GetTexture()
	{
		current_texture = SDLTextureHandle{ SDL_CreateTextureFromSurface(renderer, sprite_surface) };
		SDL_SetTextureScaleMode(current_texture.get(), SDL_ScaleMode::SDL_SCALEMODE_NEAREST);
		return current_texture.get();
	}

	void Renderer::SetPalette(const SDLPaletteHandle& palette)
	{
		SDL_SetSurfacePalette(sprite_surface, palette.get());
	}

	SDLPaletteHandle Renderer::CreatePalette(const VDPPalette& palette)
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

		sprite_surface = SDL_CreateSurface(canvas_width, canvas_height, SDL_PIXELFORMAT_INDEX8);
		s_pixel_format = sprite_surface->format;
		s_pixel_format_details = SDL_GetPixelFormatDetails(sprite_surface->format);
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
	}

	void Renderer::Render()
	{
		SDL_RenderClear(renderer);
		//SDL_ClearSurface(window_surface, 0, 0, 0, 0);
		//SDL_BlitSurfaceScaled(sprite_surface, NULL, window_surface, NULL, SDL_ScaleMode::SDL_SCALEMODE_NEAREST);
		//SDL_SetTextureScaleMode(current_texture.get(), SDL_ScaleMode::SDL_SCALEMODE_NEAREST);
		//SDL_RenderTexture(renderer, current_texture.get(), NULL, NULL);

		ImGui::Render();
		ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
		SDL_RenderPresent(renderer);
	}

	void Renderer::RenderSprite(const SpinballSprite& sprite)
	{
		sprite.RenderToSurface(sprite_surface);
	}

	SDLTextureHandle Renderer::RenderToTexture(const SpinballSprite& sprite)
	{
		sprite.RenderToSurface(sprite_surface);
		SDLTextureHandle new_texture = SDLTextureHandle{ SDL_CreateTextureFromSurface(renderer, sprite_surface) };
		SDL_SetTextureScaleMode(new_texture.get(), SDL_ScaleMode::SDL_SCALEMODE_NEAREST);
		return std::move(new_texture);
	}
}