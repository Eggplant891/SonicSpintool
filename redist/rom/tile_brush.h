#pragma once

#include "rom_data.h"
#include "types/bounding_box.h"

#include <vector>
#include <memory>

namespace spintool::rom
{
	class SpinballROM;
	struct TileSet;
	struct TileInstance;
}

namespace spintool::rom
{
	class TileBrush
	{
	public:
		TileBrush(Uint32 width, Uint32 height);

		Uint32 BrushWidth() const;
		Uint32 BrushHeight() const;
		Uint32 TotalTiles() const;

		size_t GridCoordinatesToLinearIndex(Point grid_coord) const;
		Point LinearIndexToGridCoordinates(size_t linear_index) const;

		std::vector<TileInstance> TilesFlipped(bool flip_x, bool flip_y) const;
		std::vector<TileInstance> tiles;

		constexpr static const Uint32 s_default_brush_width = 4;
		constexpr static const Uint32 s_default_brush_height = 4;
		constexpr static const Uint32 s_default_total_tiles = s_default_brush_width * s_default_brush_height;

	private:
		ROMData rom_data;

		const Uint32 m_brush_width;
		const Uint32 m_brush_height;
		const Uint32 m_brush_total_tiles;
	};

	bool operator==(const TileBrush& lhs, const TileBrush& rhs);
	bool operator==(const std::unique_ptr<TileBrush>& lhs, const std::unique_ptr<TileBrush>& rhs);

	struct TileBrushInstance
	{
		bool is_flipped_vertically = false;
		bool is_flipped_horizontally = false;
		int palette_line = 0; // 2 bit value. 0x0 -> 0x3
		Uint16 tile_brush_index = 0;
	};

	bool operator==(const TileBrushInstance& lhs, const TileBrushInstance& rhs);
	bool operator==(const std::unique_ptr<TileBrushInstance>& lhs, const std::unique_ptr<TileBrushInstance>& rhs);
}