#include "rom/game_objects/game_object_definition.h"

#include "rom/spinball_rom.h"
#include "rom/culling_tables/spline_culling_table.h"

namespace spintool::rom
{
	GameObjectDefinition GameObjectDefinition::LoadFromROM(const rom::SpinballROM& rom, Uint32 offset)
	{
		GameObjectDefinition new_instance;

		Uint32 current_offset = offset;

		new_instance.type_id = rom.ReadUint8(current_offset);
		current_offset += 1;
		new_instance.instance_id = rom.ReadUint8(current_offset);
		current_offset += 1;
		new_instance.unk_1 = rom.ReadUint8(current_offset) > 0;
		current_offset += 1;
		new_instance.unk_2 = rom.ReadUint8(current_offset) > 0;
		current_offset += 1;
		new_instance.x_pos = rom.ReadUint16(current_offset);
		current_offset += 2;
		new_instance.y_pos = rom.ReadUint16(current_offset);
		current_offset += 2;
		new_instance.collision_width = rom.ReadUint8(current_offset);
		current_offset += 1;
		new_instance.collision_height = rom.ReadUint8(current_offset);
		current_offset += 1;
		new_instance.animation_definition = rom.ReadUint32(current_offset);
		current_offset += 4;
		new_instance.collision_bbox_ptr = rom.ReadUint32(current_offset);
		current_offset += 4;
		new_instance.flags = rom.ReadUint16(current_offset);
		current_offset += 2;

		new_instance.flip_x = (new_instance.flags & 0x4000) != 0;
		new_instance.flip_y = (new_instance.flags & 0x2000) != 0;

		if (new_instance.collision_bbox_ptr != 0)
		{
			new_instance.collision = std::make_shared<rom::CollisionSpline>(rom::CollisionSpline::LoadFromROM(rom, new_instance.collision_bbox_ptr));
		}

		new_instance.rom_data.SetROMData(offset, current_offset);
		return new_instance;
	}

	Uint32 GameObjectDefinition::SaveToROM(rom::SpinballROM& writeable_rom)
	{
		Uint32 current_offset = rom_data.rom_offset;

		current_offset = writeable_rom.WriteUint8(current_offset, type_id);
		current_offset = writeable_rom.WriteUint8(current_offset, instance_id);
		current_offset = writeable_rom.WriteUint8(current_offset, unk_1);
		current_offset = writeable_rom.WriteUint8(current_offset, unk_2);
		current_offset = writeable_rom.WriteUint16(current_offset, x_pos);
		current_offset = writeable_rom.WriteUint16(current_offset, y_pos);
		current_offset = writeable_rom.WriteUint8(current_offset, collision_width);
		current_offset = writeable_rom.WriteUint8(current_offset, collision_height);
		current_offset = writeable_rom.WriteUint32(current_offset, animation_definition);
		current_offset = writeable_rom.WriteUint32(current_offset, collision_bbox_ptr);

		if (collision_bbox_ptr != 0)
		{
			collision->SaveToROM(writeable_rom, collision_bbox_ptr);
		}

		flags = flags & ~(0x4000 | 0x2000);
		if (flip_x)
		{
			flags |= 0x4000;
		}

		if (flip_y)
		{
			flags |= 0x2000;
		}

		current_offset = writeable_rom.WriteUint16(current_offset, flags);
		
		return current_offset;
	}

	bool GameObjectDefinition::FlipX() const
	{
		return flip_x;
	}

	bool GameObjectDefinition::FlipY() const
	{
		return flip_y;
	}

}