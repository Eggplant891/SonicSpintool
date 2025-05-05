#pragma once

#include "types/sdl_handle_defs.h"
#include "SDL3/SDL_stdinc.h"

#include <vector>
#include <memory>

namespace spintool::rom
{
	class SpinballROM;
	struct Sprite;
	struct SpriteTile;
	struct TileSet;
}

namespace spintool::rom
{
	struct Tile
	{
		SDLSurfaceHandle surface;
		std::vector<Uint8> pixel_data;
		Uint32 tile_index = 0;

		bool is_x_symmetrical = true;
		bool is_y_symmetrical = true;
	};

	struct TileInstance
	{
		bool is_high_priority = false;
		bool is_flipped_vertically = false;
		bool is_flipped_horizontally = false;

		int palette_line = 0; // 2 bit value. 0x0 -> 0x3
		int tile_index = 0; // 11 bit value. 0x0 -> 0x7FF

	};
	bool operator==(const TileInstance& lhs, const TileInstance& rhs);
	bool operator==(const std::unique_ptr<TileInstance>& lhs, const std::unique_ptr<TileInstance>& rhs);
}