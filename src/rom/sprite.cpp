#include "rom/sprite.h"

#include "rom/sprite_tile.h"
#include "render.h"

namespace spintool::rom
{
	BoundingBox rom::Sprite::GetBoundingBox() const
	{
		BoundingBox bounds{ std::numeric_limits<int>::max(), std::numeric_limits<int>::max(), std::numeric_limits<int>::min(), std::numeric_limits<int>::min() };

		for (const std::shared_ptr<rom::SpriteTile>& sprite_tile : sprite_tiles)
		{
			bounds.min.x = std::min<int>(sprite_tile->x_offset, bounds.min.x);
			bounds.max.x = std::max<int>(sprite_tile->x_offset + sprite_tile->x_size, bounds.max.x);

			bounds.min.y = std::min<int>(sprite_tile->y_offset, bounds.min.y);
			bounds.max.y = std::max<int>(sprite_tile->y_offset + sprite_tile->y_size, bounds.max.y);
		}

		return bounds;
	}

	void rom::Sprite::RenderToSurface(SDL_Surface* surface) const
	{
		Renderer::s_sdl_update_mutex.lock();
		SDL_LockSurface(surface);

		SDL_ClearSurface(surface, 0, 0, 0, 255);

		const BoundingBox& bounds = GetBoundingBox();

		if (bounds.Width() > 0 && bounds.Height() > 0)
		{
			for (const std::shared_ptr<rom::SpriteTile>& sprite_tile : sprite_tiles)
			{
				sprite_tile->BlitPixelDataToSurface(surface, bounds, sprite_tile->pixel_data);
			}
		}

		SDL_UnlockSurface(surface);
		Renderer::s_sdl_update_mutex.unlock();
	}

	size_t rom::Sprite::GetSizeOf() const
	{
		constexpr size_t static_size = sizeof(decltype(num_tiles))
			+ sizeof(decltype(num_vdp_tiles));

		constexpr size_t tile_size = (sizeof(decltype(rom::SpriteTile::x_offset))
			+ sizeof(decltype(rom::SpriteTile::y_offset))
			+ sizeof(decltype(rom::SpriteTile::x_size))
			+ sizeof(decltype(rom::SpriteTile::y_size))
			);

		size_t tile_data_size = 0;
		for (const std::shared_ptr<rom::SpriteTile>& sprite_tile : sprite_tiles)
		{
			tile_data_size += tile_size;
			tile_data_size += (sprite_tile->x_size / 2) * (sprite_tile->y_size);
		}

		return static_size + tile_data_size;
	}
}