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
		ROMData rom_data;
		size_t compressed_size = 0;
		size_t uncompressed_size = 0;

		Uint16 num_tiles = 0;
		std::vector<Uint8> uncompressed_data;

		std::shared_ptr<const Sprite> CreateSpriteFromTile(const size_t offset) const;

		constexpr const static Uint16 s_tile_width = 0x08;
		constexpr const static Uint16 s_tile_height = 0x08;
		constexpr const static Uint16 s_tile_total_pixels = s_tile_width * s_tile_height;
		constexpr const static Uint16 s_tile_total_bytes = (s_tile_width / 2) * s_tile_height;
	};
}