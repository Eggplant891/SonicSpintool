#pragma once

#include "rom_data.h"
#include <vector>
#include <memory>
#include <array>
#include <optional>

namespace spintool::rom
{
	class SpinballROM;
	struct TileSet;
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

	class TileBrushBase
	{
	public:
		virtual size_t BrushWidth() = 0;
		virtual size_t BrushHeight() = 0;
		size_t TotalTiles()
		{
			return BrushWidth() * BrushHeight();
		}

		ROMData rom_data;
		std::vector<TileInstance> tiles;
	};

	template<size_t width, size_t height>
	class TileBrush : public TileBrushBase
	{
	public:
		size_t BrushWidth() override { return s_brush_width; }
		size_t BrushHeight() override { return s_brush_height; }

		constexpr static const size_t s_brush_width = width;
		constexpr static const size_t s_brush_height = height;
		constexpr static const size_t s_brush_total_tiles = width * height;
	};

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

	struct TileLayout
	{
		ROMData rom_data;

		int layout_width = 0;
		int layout_height = 0;

		std::vector<std::unique_ptr<TileBrushBase>> tile_brushes;
		std::vector<TileBrushInstance> tile_brush_instances;

		std::vector<TileInstance> tile_instances;

		static std::shared_ptr<TileLayout> LoadFromROM(const SpinballROM& src_rom, size_t brushes_offset, size_t brushes_end, size_t layout_offset, std::optional<size_t> layout_end);
		static std::shared_ptr<TileLayout> LoadFromROM(const SpinballROM& src_rom, const rom::TileSet& tileset, size_t layout_offset, std::optional<size_t> layout_end);
	};
}