#pragma once

#include "rom_data.h"
#include <vector>

namespace spintool::rom
{
	struct TileInstance
	{
		bool is_high_priority = false;
		bool is_flipped_vertically = false;
		bool is_flipped_horizontally = false;

		int palette_line = 0; // 2 bit value. 0x0 -> 0x3
		int tile_index = 0; // 11 bit value. 0x0 -> 0x7FF

	};

	struct TileLayout
	{
		ROMData rom_data;

		int layout_width = 0;
		int layout_height = 0;
		std::vector<TileInstance> tile_instances;
	};
}