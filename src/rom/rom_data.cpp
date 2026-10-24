#include "rom/rom_data.h"

namespace spintool::rom
{
	void ROMData::SetROMData(const Uint32 start_offset, Uint32 end_offset)
	{
		rom_offset = start_offset;
		real_size = (end_offset - start_offset);
		rom_offset_end = start_offset + real_size;
	}

	bool ROMData::operator==(const ROMData &rhs) const
	{
		return rom_offset == rhs.rom_offset
			&& real_size == rhs.real_size
			&& rom_offset_end == rhs.rom_offset_end;
	}
}
