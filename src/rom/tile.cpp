#include "rom/tile.h"


namespace spintool::rom
{
	bool operator==(const TileInstance& lhs, const TileInstance& rhs)
	{
		return lhs.is_flipped_horizontally == rhs.is_flipped_horizontally
			&& lhs.is_flipped_vertically == rhs.is_flipped_vertically
			&& lhs.is_high_priority == rhs.is_high_priority
			&& lhs.palette_line == rhs.palette_line
			&& lhs.tile_index == rhs.tile_index;
	}

	bool operator==(const std::unique_ptr<TileInstance>& lhs, const std::unique_ptr<TileInstance>& rhs)
	{
		return lhs != nullptr && rhs != nullptr && *lhs == *rhs;
	}
}