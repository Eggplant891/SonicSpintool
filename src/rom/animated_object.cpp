#include "rom/animated_object.h"
#include "rom/spinball_rom.h"

namespace spintool::rom
{
	AnimObjectDefinition AnimObjectDefinition::LoadFromROM(const rom::SpinballROM& rom, Uint32 offset)
	{
		AnimObjectDefinition new_instance;
		Uint32 current_offset = offset;

		current_offset = rom.ReadUint32(current_offset, new_instance.sprite_table );
		current_offset = rom.ReadUint32(current_offset,new_instance.starting_animation);
		current_offset = rom.ReadUint8(current_offset, new_instance.palette_index);
		current_offset = rom.ReadUint8(current_offset,new_instance.frame_time_ticks);
		current_offset = rom.ReadUint8(current_offset, new_instance.unknown_1);
		current_offset = rom.ReadUint8(current_offset,new_instance.unknown_2);

		new_instance.rom_data.SetROMData(offset, current_offset);

		return new_instance;
	}
}
