#pragma once

#include "rom_data.h"
#include "types/bounding_box.h"
#include "rom/tile.h"
#include "rom/tile_brush.h"

#include <vector>
#include <memory>
#include <optional>

namespace spintool::rom
{
	class SpinballROM;
	struct TileSet;
}

namespace spintool::rom
{
	struct TileLayout
	{
		ROMData rom_data;

		int layout_width = 0;
		int layout_height = 0;

		std::vector<std::unique_ptr<TileBrush>> tile_brushes;
		std::vector<TileBrushInstance> tile_brush_instances;

		std::vector<TileInstance> tile_instances;

		size_t GridCoordinatesToLinearIndex(Point grid_coord) const;
		Point LinearIndexToGridCoordinates(size_t linear_index) const;

		static std::shared_ptr<TileLayout> LoadFromROM(const SpinballROM& src_rom, Uint32 layout_width, Uint32 brushes_offset, Uint32 brushes_end, Uint32 layout_offset, std::optional<Uint32> layout_end);
		static std::shared_ptr<TileLayout> LoadFromROM(const SpinballROM& src_rom, const rom::TileSet& tileset, Uint32 layout_offset, std::optional<Uint32> layout_end);
		void SaveToROM(SpinballROM& src_rom, const rom::TileSet& tile_set, Uint32 brushes_offset, Uint32 layout_offset);
		void CollapseTilesIntoBrushes(const rom::TileSet& tile_set);
		void BlitTileInstancesFromBrushInstances();
		void BlitTileBrushToLayout(const rom::TileBrush& brush, size_t brush_x_index, size_t brush_y_index, bool flip_x, bool flip_y);

		static void CacheBrushSymmetryFlags(TileLayout& tile_layout, const TileSet& tile_set);
	};
}