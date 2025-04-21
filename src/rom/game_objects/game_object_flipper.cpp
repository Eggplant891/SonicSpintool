#include "rom/game_objects/game_object_flipper.h"

#include "rom/spinball_rom.h"

namespace spintool::rom
{
	FlipperInstance FlipperInstance::LoadFromROM(const rom::SpinballROM& rom, Uint32 offset)
	{
		FlipperInstance new_instance;

		Uint32 current_offset = offset;

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

	Uint32 FlipperInstance::SaveToROM(rom::SpinballROM& writeable_rom) const
	{
		Uint32 current_offset = rom_data.rom_offset;

		current_offset = writeable_rom.WriteUint32(current_offset, animated_obj_ptr);
		current_offset = writeable_rom.WriteUint16(current_offset, x_pos);
		current_offset = writeable_rom.WriteUint16(current_offset, y_pos);
		Uint16 flags = 0;
		if (is_x_flipped)
		{
			flags |= (0x4000);
		}
		current_offset = writeable_rom.WriteUint16(current_offset, flags);

		return current_offset;
	}

	Point FlipperInstance::GetDrawPosOffset() const
	{
		return Point{ is_x_flipped ? -20 : -24, -31 };
	}

}