#pragma once

#include "rom_data.h"
#include <vector>
#include <memory>
#include <array>

namespace spintool::rom
{
	class SpinballROM;
}

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

	struct TileBrush
	{
		constexpr static const size_t s_brush_width = 4;
		constexpr static const size_t s_brush_height = 4;
		constexpr static const size_t s_brush_total_tiles = s_brush_width * s_brush_height;

		ROMData rom_data;
		std::array <TileInstance, TileBrush::s_brush_total_tiles> tiles;
	};

	struct TileBrushInstance
	{
		bool is_flipped_vertically = false;
		bool is_flipped_horizontally = false;
		int palette_line = 0; // 2 bit value. 0x0 -> 0x3
		int tile_brush_index = 0;
	};

	struct TileLayout
	{
		ROMData rom_data;

		int layout_width = 0;
		int layout_height = 0;

		std::vector<TileBrush> tile_brushes;
		std::vector<TileBrushInstance> tile_brush_instances;

		std::vector<TileInstance> tile_instances;

		static std::shared_ptr<TileLayout> LoadFromROM(const SpinballROM& src_rom, size_t brushes_offset, size_t brushes_end, size_t layout_offset, size_t layout_end);
	};
}