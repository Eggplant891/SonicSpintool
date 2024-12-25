#include "rom/tile_layout.h"

#include "rom/spinball_rom.h"

namespace spintool::rom
{
	std::shared_ptr<spintool::rom::TileLayout> TileLayout::LoadFromROM(const SpinballROM& src_rom, size_t offset)
	{
		const Uint8* start_byte = &src_rom.m_buffer[offset];
		const Uint8* current_byte = start_byte;

		auto new_layout = std::make_shared<TileLayout>();
		new_layout->layout_width = 40;//48;
		new_layout->layout_height = 28;

		const auto total_tiles = new_layout->layout_width * new_layout->layout_height;
		new_layout->tile_instances.reserve(total_tiles);

		for (size_t i = 0; i < 0x23fd /*0x146*/; ++i)
		{
			const Uint8 first_byte = *current_byte;
			++current_byte;
			const Uint8 second_byte = *current_byte;
			++current_byte;

			const Uint16 combined_bitfield = (static_cast<Uint16>(first_byte) << 8) | second_byte;

			TileInstance new_tile_instance;
			new_tile_instance.is_high_priority = (0x80 & first_byte) != 0;
			new_tile_instance.palette_line = ((0x40 | 0x20) >> 5) & first_byte;
			new_tile_instance.is_flipped_vertically = (0x10 & first_byte) != 0;
			new_tile_instance.is_flipped_horizontally = (0x08 & first_byte) != 0;
			new_tile_instance.tile_index = (static_cast<Uint16>((0x01 | 0x02 | 0x04) & first_byte) << 8) | second_byte;

			new_layout->tile_instances.emplace_back(new_tile_instance);
		}

		return new_layout;
	}
}