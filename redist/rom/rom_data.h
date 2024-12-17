#pragma once

#include "SDL3/SDL_stdinc.h"

namespace spintool::rom
{
	struct ROMData
	{
		size_t rom_offset = 0;
		size_t real_size = 0;
		size_t rom_offset_end = 0;

		void SetROMData(const size_t start_offset, size_t end_offset);
		void SetROMData(const Uint8* start_data_ptr, const Uint8* end_data_ptr, const size_t start_offset);
	};
}