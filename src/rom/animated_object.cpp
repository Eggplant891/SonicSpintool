#include "rom/animated_object.h"
#include "rom/spinball_rom.h"

namespace spintool::rom
{
	AnimObjectDefinition AnimObjectDefinition::LoadFromROM(const rom::SpinballROM& rom, size_t offset)
	{
		AnimObjectDefinition new_instance;
		size_t current_offset = offset;

		new_instance.sprite_table = rom.ReadUint32(current_offset);
		current_offset += 4;
		new_instance.starting_animation = rom.ReadUint32(current_offset);
		current_offset += 4;
		new_instance.palette_index = rom.ReadUint8(current_offset);
		current_offset += 1;
		new_instance.frame_time_ticks = rom.ReadUint8(current_offset);
		current_offset += 1;
		new_instance.unknown_1 = rom.ReadUint8(current_offset);
		current_offset += 1;
		new_instance.unknown_2 = rom.ReadUint8(current_offset);
		current_offset += 1;

		new_instance.rom_data.SetROMData(offset, current_offset);
		return new_instance;
	}
}
