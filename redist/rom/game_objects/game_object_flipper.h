#pragma once

#include "rom/rom_data.h"
#include "types/bounding_box.h"

namespace spintool::rom
{
	class SpinballROM;
}

namespace spintool::rom
{
	struct FlipperInstance
	{
		ROMData rom_data;

		Uint32 animated_obj_ptr = 0; // Unused except during runtime
		Uint16 x_pos = 0;
		Uint16 y_pos = 0;
		Uint16 flags = 0;

		bool is_x_flipped = false;

		constexpr static const size_t s_size_on_rom = 0xA;

		static constexpr int width = 44;
		static constexpr int height = 31;

		static constexpr Point dimensions{ width, height };
		Point GetDrawPosOffset() const;

		static FlipperInstance LoadFromROM(const rom::SpinballROM& rom, Uint32 offset);
		Uint32 SaveToROM(rom::SpinballROM& writeable_rom) const;

	};
}