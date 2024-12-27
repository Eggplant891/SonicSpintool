#include "rom/tile_layout.h"

#include "rom/spinball_rom.h"

namespace spintool::rom
{
	std::shared_ptr<spintool::rom::TileLayout> TileLayout::LoadFromROM(const SpinballROM& src_rom, size_t brushes_offset, size_t brushes_end, size_t layout_offset, size_t layout_end)
	{
		const Uint8* start_byte = &src_rom.m_buffer[brushes_offset];
		const Uint8* current_byte = start_byte;

		auto new_layout = std::make_shared<TileLayout>();
		new_layout->layout_width = 40;//48;
		new_layout->layout_height = 28;

		const size_t total_brushes = ((brushes_end - brushes_offset) / 2) / TileBrush::s_brush_total_tiles;
		new_layout->tile_brushes.resize(total_brushes);

		for (TileBrush& current_brush : new_layout->tile_brushes)
		{
			for (size_t i = 0; i < TileBrush::s_brush_total_tiles; ++i)
			{
				const Uint8 first_byte = *current_byte;
				++current_byte;
				const Uint8 second_byte = *current_byte;
				++current_byte;

				TileInstance& new_tile_instance = current_brush.tiles[i];
				new_tile_instance.is_high_priority = (0x80 & first_byte) != 0;
				new_tile_instance.palette_line = ((0x40 | 0x20) & first_byte) >> 5;
				new_tile_instance.is_flipped_vertically = (0x10 & first_byte) != 0;
				new_tile_instance.is_flipped_horizontally = (0x08 & first_byte) != 0;
				new_tile_instance.tile_index = (static_cast<Uint16>((0x01 | 0x02 | 0x04) & first_byte) << 8) | second_byte;
			}
		}

		current_byte = &src_rom.m_buffer[layout_offset];
		for (size_t i = 0; i < (layout_end - layout_offset) / 2; ++i)
		{
			const Uint8 first_byte = *current_byte;
			++current_byte;
			const Uint8 second_byte = *current_byte;
			++current_byte;

			TileBrushInstance brush_instance;
			brush_instance.is_flipped_horizontally = (0x08 & first_byte) != 0;
			static Uint8 first_byte_mask = 0x00;
			static Uint8 second_byte_mask = 0xFF;
			brush_instance.tile_brush_index = (static_cast<Uint16>(first_byte_mask & first_byte) << 8) | (second_byte_mask & second_byte);
			new_layout->tile_brush_instances.emplace_back(brush_instance);
		}
		return new_layout;
	}
}