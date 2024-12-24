#include "rom/sprite.h"

#include "rom/sprite_tile.h"
#include "rom/spinball_rom.h"
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

	Point rom::Sprite::GetOriginOffsetFromMinBounds() const
	{
		BoundingBox bounds = GetBoundingBox();
		return { -bounds.min.x, -bounds.min.y };
	}

	const size_t Sprite::LoadFromROM(const size_t rom_data_offset, const SpinballROM& src_rom)
	{
		const Uint8* rom_data_start = &src_rom.m_buffer[rom_data_offset];
		const Uint8* current_byte = rom_data_start;

		num_tiles = (static_cast<Sint16>(*(current_byte)) << 8) | static_cast<Sint16>(*(current_byte + 1));
		current_byte += 2;

		if (num_tiles == 0 || num_tiles > 0x80)
		{
			return rom_data_offset;
		}

		num_vdp_tiles = (static_cast<Sint16>(*(current_byte)) << 8) | static_cast<Sint16>(*(current_byte + 1));
		current_byte += 2;

		sprite_tiles.resize(num_tiles);


		for (std::shared_ptr<rom::SpriteTile>& sprite_tile : sprite_tiles)
		{
			sprite_tile = std::make_shared<rom::SpriteTile>();

			current_byte = sprite_tile->SpriteTileHeader::LoadFromROM(current_byte, (current_byte - rom_data_start) + rom_data_offset);
		}

		if (GetBoundingBox().Width() <= 1 || GetBoundingBox().Height() <= 1 || GetBoundingBox().Width() > 512 || GetBoundingBox().Height() > 512)
		{
			return rom_data_offset;
		}

		constexpr int max_tiles = 64;

		if (num_vdp_tiles > max_tiles)
		{
			return rom_data_offset;
		}

		for (std::shared_ptr<rom::SpriteTile>& sprite_tile : sprite_tiles)
		{
			const size_t total_pixels = sprite_tile->x_size * sprite_tile->y_size;
			if (total_pixels != 0)
			{
				if (rom_data_offset + ((sprite_tile->x_size / 2) * sprite_tile->y_size) < src_rom.m_buffer.size())
				{
					current_byte = sprite_tile->SpriteTileData::LoadFromROM(static_cast<const SpriteTileHeader&>(*sprite_tile), (current_byte - rom_data_start) + rom_data_offset, src_rom);
				}
			}
		}

		rom_data.SetROMData(rom_data_start, current_byte, rom_data_offset);

		return rom_data.rom_offset_end;
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
		return rom_data.real_size;
	}
}