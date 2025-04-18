#include "rom/tile_layout.h"

#include "rom/spinball_rom.h"

namespace spintool::rom
{
	std::shared_ptr<spintool::rom::TileLayout> TileLayout::LoadFromROM(const SpinballROM& src_rom, Uint32 brushes_offset, Uint32 brushes_end, Uint32 layout_offset, std::optional<Uint32> layout_end)
	{
		const Uint8* start_byte = &src_rom.m_buffer[brushes_offset];
		const Uint8* current_byte = start_byte;

		if (layout_end.has_value() == false)
		{
			return nullptr;
		}

		auto new_layout = std::make_shared<TileLayout>();

		const size_t total_brushes = (brushes_end - brushes_offset) / (TileBrush<4,4>::s_brush_total_tiles*2);
		new_layout->tile_brushes.resize(total_brushes);

		for (std::unique_ptr<TileBrushBase>& current_brush : new_layout->tile_brushes)
		{
			current_brush = std::make_unique<TileBrush<4, 4>>();
			for (size_t i = 0; i < current_brush->TotalTiles(); ++i)
			{
				const Uint8 first_byte = *current_byte;
				++current_byte;
				const Uint8 second_byte = *current_byte;
				++current_byte;

				TileInstance& new_tile_instance = current_brush->tiles.emplace_back();
				new_tile_instance.is_high_priority = (0x80 & first_byte) != 0;
				new_tile_instance.palette_line = ((0x40 | 0x20) & first_byte) >> 5;
				new_tile_instance.is_flipped_vertically = (0x10 & first_byte) != 0;
				new_tile_instance.is_flipped_horizontally = (0x08 & first_byte) != 0;
				new_tile_instance.tile_index = (static_cast<Uint16>((0x01 | 0x02 | 0x04) & first_byte) << 8) | second_byte;
			}
		}

		current_byte = &src_rom.m_buffer[layout_offset];
		for (size_t i = 0; i < (*layout_end - layout_offset) / 2; ++i)
		{
			const Uint8 first_byte = *current_byte;
			++current_byte;
			const Uint8 second_byte = *current_byte;
			++current_byte;

			TileBrushInstance brush_instance;
			brush_instance.is_flipped_vertically = (0x10 & first_byte) != 0;
			brush_instance.is_flipped_horizontally = (0x08 & first_byte) != 0;
			static Uint8 first_byte_mask = 0x03;
			static Uint8 second_byte_mask = 0xFF;
			brush_instance.tile_brush_index = (static_cast<Uint16>(first_byte & 0x03) << 8) | static_cast<Uint16>(second_byte & 0xFF);
			new_layout->tile_brush_instances.emplace_back(brush_instance);
		}

		new_layout->rom_data.SetROMData(layout_offset, *layout_end);
		return new_layout;
	}

	std::shared_ptr<spintool::rom::TileLayout> TileLayout::LoadFromROM(const SpinballROM& src_rom, const rom::TileSet& tileset, Uint32 layout_offset, std::optional<Uint32> layout_end)
	{
		auto new_layout = std::make_shared<TileLayout>();

		const Uint32 total_brushes = tileset.num_tiles;
		new_layout->tile_brushes.resize(total_brushes);

		for (Uint32 tile = 0; tile < new_layout->tile_brushes.size(); ++tile)
		{
			new_layout->tile_brushes[tile] = std::make_unique<TileBrush<1, 1>>();
			TileInstance& new_tile_instance = new_layout->tile_brushes[tile]->tiles.emplace_back();
			new_tile_instance.tile_index = static_cast<int>(tile);
		}

		const Uint8* current_byte = &src_rom.m_buffer[layout_offset];
		const Uint16 width = src_rom.ReadUint16(layout_offset);
		const Uint16 height = src_rom.ReadUint16(layout_offset+2);
		current_byte += 4;

		new_layout->layout_width = width;
		new_layout->layout_height = height;

		const auto end_address = layout_end.value_or(layout_offset + 4 + (width * 2 * height));

		for (Uint32 i = 0; i < (end_address - layout_offset) / 2; ++i)
		{
			const Uint8 first_byte = *current_byte;
			++current_byte;
			const Uint8 second_byte = *current_byte;
			++current_byte;

			TileBrushInstance brush_instance;
			//brush_instance.is_high_priority = (0x80 & first_byte) != 0;
			brush_instance.palette_line = ((0x40 | 0x20) & first_byte) >> 5;
			brush_instance.is_flipped_vertically = (0x10 & first_byte) != 0;
			brush_instance.is_flipped_horizontally = (0x08 & first_byte) != 0;
			brush_instance.tile_brush_index = (static_cast<Uint16>((0x01 | 0x02 | 0x04) & first_byte) << 8) | second_byte;
			new_layout->tile_brush_instances.emplace_back(brush_instance);
		}

		new_layout->rom_data.SetROMData(layout_offset, end_address);

		return new_layout;
	}

	void TileLayout::SaveToROM(SpinballROM& src_rom, Uint32 brushes_offset, Uint32 layout_offset)
	{
		Uint32 current_offset = brushes_offset;

		for (std::unique_ptr<TileBrushBase>& current_brush : tile_brushes)
		{
			for (const TileInstance& tile : current_brush->tiles)
			{
				Uint8 first_byte = 0;
				Uint8 second_byte = 0;


				first_byte |= tile.is_high_priority ? 0x80 : 0x00;
				first_byte |= (0x40 | 0x20) & (tile.palette_line << 5);
				first_byte |= tile.is_flipped_vertically ? 0x10 : 0x00;
				first_byte |= tile.is_flipped_horizontally ? 0x08 : 0x00;
				first_byte |= ((tile.tile_index) & static_cast<Uint16>((0x0100 | 0x0200 | 0x0400))) >> 8;

				current_offset = src_rom.WriteUint8(current_offset, first_byte);
				current_offset = src_rom.WriteUint8(current_offset, tile.tile_index & 0x00FF);

			}
		}

		current_offset = layout_offset;
		for (TileBrushInstance& brush_instance : tile_brush_instances)
		{
			//brush_instance.is_high_priority = (0x80 & first_byte) != 0;
			Uint8 first_byte = 0;

			//first_byte |= (brush_instance.palette_line << 5) & (0x40 | 0x20);
			first_byte |= brush_instance.is_flipped_vertically ? 0x10 : 0x00;
			first_byte |= brush_instance.is_flipped_horizontally ? 0x08 : 0x00;
			first_byte |= ((brush_instance.tile_brush_index) & static_cast<Uint16>((0x0100 | 0x0200 | 0x0400))) >> 8;

			current_offset = src_rom.WriteUint8(current_offset, first_byte);
			current_offset = src_rom.WriteUint8(current_offset, brush_instance.tile_brush_index & 0x00FF);
		}
	}

}