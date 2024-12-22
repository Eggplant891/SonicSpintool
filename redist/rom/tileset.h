#pragma once

#include "rom/rom_data.h"

#include "SDL3/SDL_stdinc.h"
#include <vector>
#include <memory>

namespace spintool::rom
{
	struct Sprite;
}

namespace spintool::rom
{
	struct TileSet
	{
		size_t compressed_size = 0;
		size_t uncompressed_size = 0;

		ROMData rom_data;
		std::vector<Uint8> raw_data;

		Uint16 num_tiles;

		std::shared_ptr<const Sprite> CreateSpriteFromTile(const size_t offset) const;
	};
}