#pragma once

#include "types/bounding_box.h"
#include "rom/sprite_tile.h"

#include "SDL3/SDL_stdinc.h"

#include <vector>
#include <memory>

struct SDL_Surface;

namespace spintool::rom
{
	struct Sprite
	{
		size_t rom_offset = 0;
		size_t rom_offset_end = 0;
		size_t real_size = 0;
		size_t unrealised_size = 0;

		Uint16 num_tiles = 0;
		Uint16 num_vdp_tiles = 0;

		std::vector<std::shared_ptr<rom::SpriteTile>> sprite_tiles;

		void RenderToSurface(SDL_Surface* surface) const;

		BoundingBox GetBoundingBox() const;
		Point GetOriginOffsetFromMinBounds() const;
		size_t GetSizeOf() const; // Size in bytes on ROM
	};
}