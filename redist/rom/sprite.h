#pragma once

#include "types/bounding_box.h"
#include "rom/sprite_tile.h"
#include "rom/rom_data.h"

#include "SDL3/SDL_stdinc.h"

#include <vector>
#include <memory>

struct SDL_Surface;

namespace spintool::rom
{
	class SpinballROM;
}
namespace spintool::rom
{
	struct Sprite
	{
		Uint16 num_tiles = 0;
		Uint16 num_vdp_tiles = 0;

		std::vector<std::shared_ptr<rom::SpriteTile>> sprite_tiles;

		ROMData rom_data;
		bool is_valid = false;
		const size_t LoadFromROM(const size_t rom_data_offset, const SpinballROM& src_rom);

		void RenderToSurface(SDL_Surface* surface) const;

		BoundingBox GetBoundingBox() const;
		Point GetOriginOffsetFromMinBounds() const;
		size_t GetSizeOf() const; // Size in bytes on ROM
	};
}