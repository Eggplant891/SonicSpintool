#pragma once

#include "rom/rom_data.h"

namespace spintool::rom
{
	class SpinballROM;
}

namespace spintool::rom
{
	struct RingInstance
	{
		ROMData rom_data;

		Uint16 instance_id = 0;
		Uint16 x_pos = 0;
		Uint16 y_pos = 0;

		constexpr static const size_t s_size_on_rom = 0x4;

		static RingInstance LoadFromROM(const rom::SpinballROM& rom, size_t offset);
	};
}