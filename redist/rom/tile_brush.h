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
	class TileBrushBase
	{
	public:
		virtual Uint32 BrushWidth() const = 0;
		virtual Uint32 BrushHeight() const = 0;
		Uint32 TotalTiles() const
		{
			return BrushWidth() * BrushHeight();
		}

		std::vector<TileInstance> TilesFlipped(bool flip_x, bool flip_y) const;

		size_t GridCoordinatesToLinearIndex(Point grid_coord) const;
		Point LinearIndexToGridCoordinates(size_t linear_index) const;

		ROMData rom_data;
		std::vector<TileInstance> tiles;
	};

	template<Uint32 width, Uint32 height>
	class TileBrush : public TileBrushBase
	{
	public:
		Uint32 BrushWidth() const override { return s_brush_width; }
		Uint32 BrushHeight() const override { return s_brush_height; }

		constexpr static const Uint32 s_brush_width = width;
		constexpr static const Uint32 s_brush_height = height;
		constexpr static const Uint32 s_brush_total_tiles = width * height;
	};

	bool operator==(const TileBrushBase& lhs, const TileBrushBase& rhs);
	bool operator==(const std::unique_ptr<TileBrushBase>& lhs, const std::unique_ptr<TileBrushBase>& rhs);


	template class TileBrush<1, 1>;
	template class TileBrush<4, 4>;
	template class TileBrush<8, 8>;

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