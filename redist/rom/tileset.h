#pragma once

#include "SDL3/SDL_stdinc.h"
#include <vector>

namespace spintool::rom
{
	struct TileSet
	{
		size_t rom_offset = 0;
		size_t rom_offset_end = 0;

		size_t compressed_size = 0;
		size_t uncompressed_size = 0;

		Uint16 num_tiles;
		std::vector<unsigned char> data;
	};
}