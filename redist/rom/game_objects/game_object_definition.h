#pragma once

#include "rom/rom_data.h"
#include "types/rom_ptr.h"

namespace spintool::rom
{
	class SpinballROM;
}

namespace spintool::rom
{
	struct GameObjectDefinition
	{
		ROMData rom_data;

		Uint8 type_id = 0;
		Uint8 instance_id = 0;
		Uint8 unk_1 = 0;
		Uint8 unk_2 = 0;
		Uint16 x_pos = 0;
		Uint16 y_pos = 0;
		Uint8 collision_width = 0;
		Uint8 collision_height = 0;
		Ptr32 animation_definition = 0;
		Ptr32 animation_ptr = 0;
		Uint16 flags = 0;

		bool FlipX() const;
		bool FlipY() const;

		constexpr static const size_t s_size_on_rom = 0x14;

		static GameObjectDefinition LoadFromROM(const rom::SpinballROM& rom, Uint32 offset);
		Uint32 SaveToROM(rom::SpinballROM& writeable_rom);
	};
}