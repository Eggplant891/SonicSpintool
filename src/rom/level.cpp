#include "rom/level.h"

#include "rom/rom_asset_definitions.h"
#include "rom/spinball_rom.h"
#include "rom/culling_tables/spline_culling_table.h"
#include "rom/culling_tables/game_obj_collision_culling_table.h"
#include "rom/culling_tables/animated_object_culling_table.h"

#include <vector>
#include <numeric>

namespace spintool::rom
{

	Level Level::LoadFromROM(const rom::SpinballROM& target_rom, int level_index)
	{
		Level new_level;

		rom::LevelDataOffsets level_data_offsets{ level_index };

		rom::Ptr32 level_name_offset = target_rom.ReadUint32(level_data_offsets.level_name);
		const char* level_name = reinterpret_cast<const char*>(&target_rom.m_buffer[level_name_offset]);

		char buffer[256];
		sprintf(buffer, "%s", level_name);
		new_level.m_level_name = buffer;
		new_level.m_level_index = level_index;

		new_level.m_ring_instances.clear();
		new_level.m_ring_instances.reserve(level_data_offsets.ring_instances.count);

		{
			Uint32 current_table_offset = level_data_offsets.ring_instances.offset;

			for (Uint32 i = 0; i < level_data_offsets.ring_instances.count; ++i)
			{
				new_level.m_ring_instances.emplace_back(rom::RingInstance::LoadFromROM(target_rom, current_table_offset));
				current_table_offset = new_level.m_ring_instances.back().rom_data.rom_offset_end;
			}
		}

		new_level.m_data_offsets = level_data_offsets;
		new_level.m_spline_culling_table = std::make_unique<rom::SplineCullingTable>(rom::SplineCullingTable::LoadFromROM(target_rom, target_rom.ReadUint32(new_level.m_data_offsets.collision_data_terrain)));

		return new_level;
	}

	rom::Ptr32 Level::SaveToROM(rom::SpinballROM& target_rom) const
	{
		//for (TileLayer& layer : m_tile_layers)
		{
			m_tile_layers[0].tile_layout->SaveToROM(target_rom, *m_tile_layers[0].tileset, target_rom.ReadUint32(m_data_offsets.background_tile_brushes), target_rom.ReadUint32(m_data_offsets.background_tile_layout));
			m_tile_layers[1].tile_layout->SaveToROM(target_rom, *m_tile_layers[1].tileset, target_rom.ReadUint32(m_data_offsets.foreground_tile_brushes), target_rom.ReadUint32(m_data_offsets.foreground_tile_layout));
		}

		m_spline_culling_table->SaveToROM(target_rom, target_rom.ReadUint32(m_data_offsets.collision_data_terrain));
		if (m_data_offsets.collision_tile_obj_ids.offset != 0) // This is 0 for Showdown, which doesn't supply a culling table!
		{
			const rom::Ptr32 obj_end = m_game_object_culling_table->SaveToROM(target_rom, m_data_offsets.collision_tile_obj_ids.offset);
			target_rom.WriteUint32(m_data_offsets.camera_activation_sector_anim_obj_ids, obj_end);
		}
		m_anim_object_culling_table->SaveToROM(target_rom, target_rom.ReadUint32(m_data_offsets.camera_activation_sector_anim_obj_ids));

		for (const rom::FlipperInstance& obj : m_flipper_instances)
		{
			obj.SaveToROM(target_rom);
		}

		for (const rom::RingInstance& obj : m_ring_instances)
		{
			obj.SaveToROM(target_rom);
		}

		const Uint8 num_emeralds = std::accumulate(std::begin(m_game_obj_instances), std::end(m_game_obj_instances), static_cast<Uint8>(0),
			[](const Uint8 running_val, const rom::GameObjectDefinition& game_obj)
			{
				if (game_obj.instance_id != 0 && game_obj.type_id == 0x6)
				{
					return running_val + 1;
				}
				return running_val + 0;
			});
		target_rom.WriteUint8(m_data_offsets.emerald_count, num_emeralds);

		return 0;
	}
}
