#include "rom/game_objects/game_object_flipper.h"

#include "rom/spinball_rom.h"

namespace spintool::rom
{
	FlipperInstance FlipperInstance::LoadFromROM(const rom::SpinballROM& rom, size_t offset)
	{
		FlipperInstance new_instance;

		size_t current_offset = offset;

		new_instance.animated_obj_ptr = rom.ReadUint32(current_offset);
		current_offset += 4;
		new_instance.x_pos = rom.ReadUint16(current_offset);
		current_offset += 2;
		new_instance.y_pos = rom.ReadUint16(current_offset);
		current_offset += 2;
		new_instance.flags = rom.ReadUint16(current_offset);
		current_offset += 2;
		if ((new_instance.flags & 0x4000) == 0x4000)
		{
			new_instance.is_x_flipped = true;
		}
		new_instance.rom_data.SetROMData(offset, current_offset);

		return new_instance;
	}
}