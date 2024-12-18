#pragma once

#include "SDL3/SDL_stdinc.h"
#include <vector>
#include "rom/rom_data.h"

namespace spintool::rom
{
	struct TileSet
	{
		size_t compressed_size = 0;
		size_t uncompressed_size = 0;

		Uint16 num_tiles;
		std::vector<unsigned char> data;

		ROMData rom_data;
	};
}