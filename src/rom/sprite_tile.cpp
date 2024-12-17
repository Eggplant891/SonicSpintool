#include "rom/sprite_tile.h"

#include "render.h"

namespace spintool::rom
{
	void rom::SpriteTile::RenderToSurface(SDL_Surface* surface) const
	{
		Renderer::s_sdl_update_mutex.lock();
		SDL_LockSurface(surface);

		SDL_ClearSurface(surface, 0, 0, 0, 0);

		const BoundingBox& bounds{ x_offset, y_offset, x_offset + x_size, y_offset + y_size };

		if (bounds.Width() > 0 && bounds.Height() > 0)
		{
			BlitPixelDataToSurface(surface, bounds, pixel_data);
		}

		SDL_UnlockSurface(surface);
		Renderer::s_sdl_update_mutex.unlock();
	}

	void rom::SpriteTile::BlitPixelDataToSurface(SDL_Surface* surface, const BoundingBox& bounds, const std::vector<Uint32>& pixels_data) const
	{
		int x_off = (x_offset - bounds.min.x);
		int y_off = (y_offset - bounds.min.y);
		int x_max = x_off + x_size;
		int y_max = y_off + y_size;

		size_t target_pixel_index = (y_off * surface->pitch) + x_off;

		for (size_t i = 0; i < pixels_data.size() && i < surface->pitch * surface->h && i / x_max < y_max; ++i, target_pixel_index += 1)
		{
			if ((i % x_size) == 0)
			{
				target_pixel_index = (y_off * surface->pitch) + (surface->pitch * (i / x_size)) + x_off;
			}

			//*(Uint32*)(&((Uint8*)surface->pixels)[target_pixel_index]) = SDL_MapRGB(Renderer::s_pixel_format_details, NULL, static_cast<Uint8>((pixel_data[i] / 16.0f) * 255), 0, 0);
			((Uint8*)surface->pixels)[target_pixel_index] = pixels_data[i];
		}
	}

}