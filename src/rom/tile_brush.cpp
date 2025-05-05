#include "rom/tile_brush.h"
#include "rom/tile.h"

namespace spintool::rom
{
	bool operator==(const TileBrush& lhs, const TileBrush& rhs)
	{
		return lhs.BrushWidth() == rhs.BrushWidth()
			&& lhs.BrushHeight() == rhs.BrushHeight()
			&& lhs.tiles == rhs.tiles;
	}

	bool operator==(const std::unique_ptr<TileBrush>& lhs, const std::unique_ptr<TileBrush>& rhs)
	{
		return lhs != nullptr && rhs != nullptr && *lhs == *rhs;
	}

	bool operator==(const TileBrushInstance& lhs, const TileBrushInstance& rhs)
	{
		return lhs.is_flipped_horizontally == rhs.is_flipped_horizontally
			&& lhs.is_flipped_vertically == rhs.is_flipped_vertically
			&& lhs.palette_line == rhs.palette_line
			&& lhs.tile_brush_index == rhs.tile_brush_index;
	}

	bool operator==(const std::unique_ptr<TileBrushInstance>& lhs, const std::unique_ptr<TileBrushInstance>& rhs)
	{
		return lhs != nullptr && rhs != nullptr && *lhs == *rhs;
	}

	Uint32 TileBrush::BrushWidth() const
	{
		return m_brush_width;
	}

	Uint32 TileBrush::BrushHeight() const
	{
		return m_brush_height;
	}

	Uint32 TileBrush::TotalTiles() const
	{
		return BrushWidth() * BrushHeight();
	}

	std::vector<rom::TileInstance> TileBrush::TilesFlipped(bool flip_x, bool flip_y) const
	{
		std::vector<rom::TileInstance> out_tiles = tiles;

		if (flip_x)
		{
			for (size_t x = 0; x < BrushWidth() / 2; ++x)
			{
				for (size_t y = 0; y < BrushHeight(); ++y)
				{
					const size_t x_offset_inverse = ((BrushWidth() - 1) - x);
					const size_t y_offset = y * BrushWidth();
					rom::TileInstance& lhs = out_tiles[y_offset + x];
					rom::TileInstance& rhs = out_tiles[y_offset + x_offset_inverse];
					lhs.is_flipped_horizontally = !lhs.is_flipped_horizontally;
					rhs.is_flipped_horizontally = !rhs.is_flipped_horizontally;
					std::swap(lhs, rhs);
				}
			}
		}

		if (flip_y)
		{
			for (size_t x = 0; x < BrushWidth(); ++x)
			{
				for (size_t y = 0; y < BrushHeight() / 2; ++y)
				{
					const size_t y_offset = y * BrushWidth();
					const size_t y_offset_inverse = ((BrushHeight() - 1) - y) * BrushWidth();
					rom::TileInstance& lhs = out_tiles[y_offset + x];
					rom::TileInstance& rhs = out_tiles[y_offset_inverse + x];
					lhs.is_flipped_vertically = !lhs.is_flipped_vertically;
					rhs.is_flipped_vertically = !rhs.is_flipped_vertically;
					std::swap(lhs, rhs);
				}
			}
		}

		return out_tiles;
	}

	size_t TileBrush::GridCoordinatesToLinearIndex(Point grid_coord) const
	{
		return grid_coord.x + (grid_coord.y * BrushWidth());

	}

	Point TileBrush::LinearIndexToGridCoordinates(size_t linear_index) const
	{
		Point out_coord{};
		out_coord.x = (linear_index % BrushWidth());
		out_coord.y = static_cast<int>(((linear_index - (linear_index % BrushWidth())) / static_cast<float>(BrushWidth())));

		return out_coord;
	}

	TileBrush::TileBrush(Uint32 width, Uint32 height)
		: m_brush_width(width)
		, m_brush_height(height)
		, m_brush_total_tiles(width * height)
	{

	}

	void TileBrush::AssignTiles(const std::vector<TileInstance>& source_tiles)
	{
		if (source_tiles.size() == tiles.size())
		{
			tiles = source_tiles;
		}
	}

}