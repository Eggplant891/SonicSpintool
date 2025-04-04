#include "editor/level.h"

#include "rom/rom_asset_definitions.h"
#include "rom/spinball_rom.h"

namespace spintool
{

	Level Level::LoadFromROM(const rom::SpinballROM& rom, int level_index)
	{
		Level new_level;

		rom::LevelDataOffsets level_data_offsets{ level_index };

		rom::Ptr32 level_name_offset = rom.ReadUint32(level_data_offsets.level_name);
		const char* level_name = reinterpret_cast<const char*>(&rom.m_buffer[level_name_offset]);

		char buffer[256];
		sprintf_s(buffer, "%s", level_name);
		new_level.m_level_name = buffer;

		new_level.m_ring_instances.clear();
		new_level.m_ring_instances.reserve(level_data_offsets.ring_instances.count);

		{
			Uint32 current_table_offset = level_data_offsets.ring_instances.offset;

			for (Uint32 i = 0; i < level_data_offsets.ring_instances.count; ++i)
			{
				new_level.m_ring_instances.emplace_back(rom::RingInstance::LoadFromROM(rom, current_table_offset));
				current_table_offset = new_level.m_ring_instances.back().rom_data.rom_offset_end;
			}
		}

		return new_level;
	}

	rom::Ptr32 Level::SaveToROM(rom::SpinballROM& rom, int level_index) const
	{
		rom; level_index;

		return 0;
	}

}