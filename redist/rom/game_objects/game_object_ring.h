#pragma once

#include "rom/rom_data.h"
#include "types/bounding_box.h"

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

		constexpr static const Uint32 s_size_on_rom = 0x4;

		static constexpr int width = 16;
		static constexpr int height = 16;

		static constexpr ImVec2 dimensions{ width, height };
		Point draw_pos_offset{ -8 , -16 };

		static RingInstance LoadFromROM(const rom::SpinballROM& rom, Uint32 offset);
		Uint32 SaveToROM(rom::SpinballROM& writeable_rom) const;
	};
}