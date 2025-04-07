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
		virtual Uint32 BrushWidth() = 0;
		virtual Uint32 BrushHeight() = 0;
		Uint32 TotalTiles()
		{
			return BrushWidth() * BrushHeight();
		}

		ROMData rom_data;
		std::vector<TileInstance> tiles;
	};

	template<Uint32 width, Uint32 height>
	class TileBrush : public TileBrushBase
	{
	public:
		Uint32 BrushWidth() override { return s_brush_width; }
		Uint32 BrushHeight() override { return s_brush_height; }

		constexpr static const Uint32 s_brush_width = width;
		constexpr static const Uint32 s_brush_height = height;
		constexpr static const Uint32 s_brush_total_tiles = width * height;
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

		static std::shared_ptr<TileLayout> LoadFromROM(const SpinballROM& src_rom, Uint32 brushes_offset, Uint32 brushes_end, Uint32 layout_offset, std::optional<Uint32> layout_end);
		static std::shared_ptr<TileLayout> LoadFromROM(const SpinballROM& src_rom, const rom::TileSet& tileset, Uint32 layout_offset, std::optional<Uint32> layout_end);

		void SaveToROM(SpinballROM& src_rom, Uint32 brushes_offset, Uint32 layout_offset);
	};
}