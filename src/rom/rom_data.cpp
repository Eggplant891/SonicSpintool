#include "rom/rom_data.h"

namespace spintool::rom
{
	void ROMData::SetROMData(const size_t start_offset, size_t end_offset)
	{
		rom_offset = start_offset;
		real_size = (end_offset - start_offset);
		rom_offset_end = start_offset + real_size;
	}

	void ROMData::SetROMData(const Uint8* start_data_ptr, const Uint8* end_data_ptr, const size_t start_offset)
	{
		rom_offset = start_offset;
		real_size = (end_data_ptr - start_data_ptr);
		rom_offset_end = start_offset + real_size ;
	}

}
