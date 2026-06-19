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

	std::shared_ptr<const Sprite> Sprite::LoadFromROM(
		const SpinballROM& src_rom,
		Uint32 offset
	)
	{
		const size_t rom_size = src_rom.m_buffer.size();
		if (offset > rom_size || rom_size - offset < 4)
		{
			return nullptr;
		}

		const Uint8* const rom_data_start =
			src_rom.m_buffer.data() + offset;
		const Uint8* current_byte = rom_data_start;

		auto read_be16 = [](const Uint8* bytes) -> Uint16
		{
			return static_cast<Uint16>(
				(static_cast<Uint16>(bytes[0]) << 8) |
				static_cast<Uint16>(bytes[1])
			);
		};

		std::shared_ptr<rom::Sprite> new_sprite =
			std::make_shared<rom::Sprite>();

		new_sprite->num_tiles = read_be16(current_byte);
		current_byte += 2;

		if (new_sprite->num_tiles == 0 || new_sprite->num_tiles > 0x80)
		{
			return nullptr;
		}

		new_sprite->num_vdp_tiles = read_be16(current_byte);
		current_byte += 2;

		constexpr Uint16 max_vdp_tiles = 1024;
		if (
			new_sprite->num_vdp_tiles == 0 ||
			new_sprite->num_vdp_tiles > max_vdp_tiles
		)
		{
			return nullptr;
		}

		constexpr size_t sprite_tile_header_size = 6;
		const size_t header_bytes =
			static_cast<size_t>(new_sprite->num_tiles) *
			sprite_tile_header_size;

		const size_t bytes_consumed = static_cast<size_t>(
			current_byte - rom_data_start
		);
		if (
			bytes_consumed > rom_size - offset ||
			header_bytes > rom_size - offset - bytes_consumed
		)
		{
			return nullptr;
		}

		new_sprite->sprite_tiles.resize(new_sprite->num_tiles);

		for (std::shared_ptr<rom::SpriteTile>& sprite_tile :
			new_sprite->sprite_tiles)
		{
			sprite_tile = std::make_shared<rom::SpriteTile>();
			current_byte = sprite_tile->SpriteTileHeader::LoadFromROM(
				current_byte,
				static_cast<Uint32>(current_byte - rom_data_start) + offset
			);
		}

		if (std::any_of(
			std::begin(new_sprite->sprite_tiles),
			std::end(new_sprite->sprite_tiles),
			[](const std::shared_ptr<rom::SpriteTile>& tile)
			{
				return
					tile->x_size == 0 ||
					tile->x_size > 32 ||
					tile->y_size == 0 ||
					tile->y_size > 32;
			}
		))
		{
			return nullptr;
		}

		Uint32 calculated_vdp_tiles = 0;
		for (const std::shared_ptr<rom::SpriteTile>& sprite_tile :
			new_sprite->sprite_tiles)
		{
			calculated_vdp_tiles +=
				(static_cast<Uint32>(sprite_tile->x_size) / 8U) *
				(static_cast<Uint32>(sprite_tile->y_size) / 8U);
		}

		if (
			calculated_vdp_tiles == 0 ||
			calculated_vdp_tiles > max_vdp_tiles
		)
		{
			return nullptr;
		}

		for (std::shared_ptr<rom::SpriteTile>& sprite_tile :
			new_sprite->sprite_tiles)
		{
			const size_t tile_data_size =
				(static_cast<size_t>(sprite_tile->x_size) *
				 static_cast<size_t>(sprite_tile->y_size)) /
				2U;

			const size_t current_offset = static_cast<size_t>(
				current_byte - rom_data_start
			);
			if (
				current_offset > rom_size - offset ||
				tile_data_size > rom_size - offset - current_offset
			)
			{
				return nullptr;
			}

			current_byte = sprite_tile->SpriteTileData::LoadFromROM(
				static_cast<const SpriteTileHeader&>(*sprite_tile),
				static_cast<Uint32>(current_offset) + offset,
				src_rom
			);
		}

		new_sprite->rom_data.SetROMData(
			rom_data_start,
			current_byte,
			offset
		);
		new_sprite->is_valid = true;

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
