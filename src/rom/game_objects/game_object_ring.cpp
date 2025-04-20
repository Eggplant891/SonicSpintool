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

	Uint32 RingInstance::SaveToROM(rom::SpinballROM& writeable_rom) const
	{
		Uint32 current_offset = rom_data.rom_offset;

		current_offset = writeable_rom.WriteUint16(current_offset, instance_id);
		current_offset = writeable_rom.WriteUint16(current_offset, x_pos);
		current_offset = writeable_rom.WriteUint16(current_offset, y_pos);

		return current_offset;
	}

}