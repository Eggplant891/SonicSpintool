#include "rom/sprite.h"

#include "rom/sprite_tile.h"
#include "rom/spinball_rom.h"
#include "render.h"

namespace spintool::rom
{
	BoundingBox rom::Sprite::GetBoundingBox() const
	{
		BoundingBox bounds{ std::numeric_limits<int>::max(), std::numeric_limits<int>::max(), std::numeric_limits<int>::min(), std::numeric_limits<int>::min() };

		for (const std::shared_ptr<const rom::SpriteTile>& sprite_tile : sprite_tiles)
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

	std::shared_ptr<const Sprite> Sprite::LoadFromROM(const SpinballROM& src_rom, Uint32 offset)
	{
		if (offset + 4 < offset) // overflow detection
		{
			return nullptr;
		}

		size_t next_byte_offset = offset;

		std::shared_ptr<rom::Sprite> new_sprite = std::make_shared<rom::Sprite>();

		const Uint8* rom_data_start = &src_rom.m_buffer[offset];
		const Uint8* current_byte = rom_data_start;

		new_sprite->num_tiles = (static_cast<Sint16>(*(current_byte)) << 8) | static_cast<Sint16>(*(current_byte + 1));
		current_byte += 2;

		if (new_sprite->num_tiles == 0 || new_sprite->num_tiles > 0x80)
		{
			return nullptr;
		}

		new_sprite->num_vdp_tiles = (static_cast<Sint16>(*(current_byte)) << 8) | static_cast<Sint16>(*(current_byte + 1));
		current_byte += 2;

		new_sprite->sprite_tiles.resize(new_sprite->num_tiles);


		for (std::shared_ptr<rom::SpriteTile>& sprite_tile : new_sprite->sprite_tiles)
		{
			sprite_tile = std::make_shared<rom::SpriteTile>();

			current_byte = sprite_tile->SpriteTileHeader::LoadFromROM(current_byte, static_cast<Uint32>(current_byte - rom_data_start) + offset);
		}

		if (new_sprite->num_tiles == 0)
		{
			return nullptr;
		}

		if (std::any_of(std::begin(new_sprite->sprite_tiles), std::end(new_sprite->sprite_tiles), [](const std::shared_ptr<rom::SpriteTile>& tile)
			{
				return tile->x_size == 0 || tile->x_size > 32 || tile->y_size == 0 || tile->y_size > 32;
			}))
		{
			return nullptr;
		}

		constexpr int max_tiles = 64;

		if (new_sprite->num_vdp_tiles > max_tiles)
		{
			return nullptr;
		}

		for (std::shared_ptr<rom::SpriteTile>& sprite_tile : new_sprite->sprite_tiles)
		{
			const Uint32 total_pixels = sprite_tile->x_size * sprite_tile->y_size;
			if (total_pixels != 0)
			{
				if (offset + ((sprite_tile->x_size / 2) * sprite_tile->y_size) < src_rom.m_buffer.size())
				{
					current_byte = sprite_tile->SpriteTileData::LoadFromROM(static_cast<const SpriteTileHeader&>(*sprite_tile), static_cast<Uint32>(current_byte - rom_data_start) + offset, src_rom);
				}
			}
		}

		new_sprite->rom_data.SetROMData(rom_data_start, current_byte, offset);

		return new_sprite;
	}

	void rom::Sprite::RenderToSurface(SDL_Surface* surface) const
	{
		const BoundingBox& bounds = GetBoundingBox();

		Renderer::s_sdl_update_mutex.lock();
		SDL_ClearSurface(surface, 0, 0, 0, 0);
		if (bounds.Width() > 0 && bounds.Height() > 0)
		{
			for (const std::shared_ptr<rom::SpriteTile>& sprite_tile : sprite_tiles)
			{
				sprite_tile->BlitPixelDataToSurface(surface, bounds, sprite_tile->pixel_data);
			}
		}
		Renderer::s_sdl_update_mutex.unlock();
	}

	Uint32 rom::Sprite::GetSizeOf() const
	{
		return rom_data.real_size;
	}
}