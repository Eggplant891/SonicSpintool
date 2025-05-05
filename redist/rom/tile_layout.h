#pragma once

#include "rom_data.h"
#include "types/bounding_box.h"

#include <vector>
#include <memory>
#include <optional>

namespace spintool::rom
{
	class SpinballROM;
	struct TileSet;
	struct TileInstance;
	class TileBrushBase;
	struct TileBrushInstance;
}

namespace spintool::rom
{
	struct TileLayout
	{
		ROMData rom_data;

		int layout_width = 0;
		int layout_height = 0;

		std::vector<std::unique_ptr<TileBrushBase>> tile_brushes;
		std::vector<TileBrushInstance> tile_brush_instances;

		std::vector<TileInstance> tile_instances;

		size_t GridCoordinatesToLinearIndex(Point grid_coord) const;
		Point LinearIndexToGridCoordinates(size_t linear_index) const;

		static std::shared_ptr<TileLayout> LoadFromROM(const SpinballROM& src_rom, Uint32 layout_width, Uint32 brushes_offset, Uint32 brushes_end, Uint32 layout_offset, std::optional<Uint32> layout_end);
		static std::shared_ptr<TileLayout> LoadFromROM(const SpinballROM& src_rom, const rom::TileSet& tileset, Uint32 layout_offset, std::optional<Uint32> layout_end);
		void SaveToROM(SpinballROM& src_rom, Uint32 brushes_offset, Uint32 layout_offset);
		void CollapseTilesIntoBrushes();
		void BlitTileInstancesFromBrushInstances();
	};
}