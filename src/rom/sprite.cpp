#include "rom/sprite.h"

#include "rom/sprite_tile.h"
#include "rom/spinball_rom.h"
#include "render.h"

namespace spintool::rom
{
	BoundingBox rom::Sprite::GetBoundingBox() const
	{
		BoundingBox bounds{ std::numeric_limits<int>::max(), std::numeric_limits<int>::max(), std::numeric_limits<int>::min(), std::numeric_limits<int>::min() };

		for (const auto& sprite_tile : sprite_tiles)
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
		auto new_sprite = std::make_shared<rom::Sprite>();
		auto current_offset = offset;
		current_offset = src_rom.ReadUint16(current_offset, new_sprite->num_tiles);
		current_offset = src_rom.ReadUint16(current_offset, new_sprite->num_vdp_tiles);
		new_sprite->sprite_tiles.resize(new_sprite->num_tiles);

		for (auto& sprite_tile : new_sprite->sprite_tiles)
		{
			sprite_tile = std::make_shared<rom::SpriteTile>();
			current_offset = sprite_tile->SpriteTileHeader::LoadFromROM(src_rom, current_offset);
		}

		if (std::any_of(
			std::begin(new_sprite->sprite_tiles),
			std::end(new_sprite->sprite_tiles),
			[](const auto& tile)
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
		for (const auto& sprite_tile : new_sprite->sprite_tiles)
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

		for (auto& sprite_tile : new_sprite->sprite_tiles)
		{
			current_offset = sprite_tile->SpriteTileData::LoadFromROM(static_cast<const SpriteTileHeader&>(*sprite_tile), src_rom, current_offset);
		}

		new_sprite->rom_data.SetROMData(offset, current_offset);
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
			// The first mapping piece has the highest sprite-to-sprite priority on
			// Mega Drive. Draw in reverse so earlier pieces remain in front.
			for (auto iterator = sprite_tiles.rbegin();
				 iterator != sprite_tiles.rend();
				 ++iterator)
			{
				const auto& sprite_tile = *iterator;
				sprite_tile->BlitPixelDataToSurface(
					surface,
					bounds,
					sprite_tile->pixel_data
				);
			}
		}
		Renderer::s_sdl_update_mutex.unlock();
	}

	Uint32 rom::Sprite::GetSizeOf() const
	{
		return rom_data.real_size;
	}
}
