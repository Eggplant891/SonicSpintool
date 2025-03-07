#include "rom/game_objects/game_object_ring.h"

#include "rom/spinball_rom.h"

namespace spintool::rom
{
	RingInstance RingInstance::LoadFromROM(const rom::SpinballROM& rom, Uint32 offset)
	{
		RingInstance new_instance;

		Uint32 current_offset = offset;

		new_instance.instance_id = rom.ReadUint16(current_offset);
		current_offset += 2;
		new_instance.x_pos = rom.ReadUint16(current_offset);
		current_offset += 2;
		new_instance.y_pos = rom.ReadUint16(current_offset);
		current_offset += 2;

		new_instance.rom_data.SetROMData(offset, current_offset);
		return new_instance;
	}

}