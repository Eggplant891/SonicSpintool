#include "rom/rom_data.h"

namespace spintool::rom
{
	void ROMData::SetROMData(const Uint32 start_offset, Uint32 end_offset)
	{
		rom_offset = start_offset;
		real_size = (end_offset - start_offset);
		rom_offset_end = start_offset + real_size;
	}

	void ROMData::SetROMData(const Uint8* start_data_ptr, const Uint8* end_data_ptr, const Uint32 start_offset)
	{
		rom_offset = start_offset;
		real_size = static_cast<Uint32>(end_data_ptr - start_data_ptr);
		rom_offset_end = start_offset + real_size ;
	}

}
