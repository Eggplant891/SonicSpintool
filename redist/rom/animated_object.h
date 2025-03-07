#pragma once
#include "types/rom_ptr.h"
#include "rom_data.h"

namespace spintool::rom
{
	class SpinballROM;
}

namespace spintool::rom
{
	struct AnimObjectDefinition
	{
		ROMData rom_data;

		Ptr32 sprite_table = 0;
		Ptr32 starting_animation = 0;
		Uint8 palette_index = 0;
		Uint8 frame_time_ticks = 0;
		Uint8 unknown_1 = 0;
		Uint8 unknown_2 = 0;

		constexpr static const size_t s_size_on_rom = 0xC;

		static AnimObjectDefinition LoadFromROM(const rom::SpinballROM& rom, Uint32 offset);
	};
}