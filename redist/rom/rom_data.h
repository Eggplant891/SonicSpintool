#pragma once

#include "SDL3/SDL_stdinc.h"

namespace spintool::rom
{
	struct ROMData
	{
		Uint32 rom_offset = 0;
		Uint32 real_size = 0;
		Uint32 rom_offset_end = 0;

		void SetROMData(Uint32 start_offset, Uint32 end_offset);
		void SetROMData(const Uint8* start_data_ptr, const Uint8* end_data_ptr, Uint32 start_offset);

		bool operator==(const ROMData& rhs) const;
	};
}