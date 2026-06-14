#include "rom/level.h"

#include "rom/rom_asset_definitions.h"
#include "rom/spinball_rom.h"
#include "rom/culling_tables/spline_culling_table.h"
#include "rom/culling_tables/game_obj_collision_culling_table.h"
#include "rom/culling_tables/animated_object_culling_table.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <numeric>
#include <vector>

namespace spintool::rom
{
	namespace
	{
		constexpr int level_count = 4;
		constexpr std::size_t max_level_name_length = 255;
		constexpr std::size_t ring_instance_size = 6;

		bool RangeIsValid(const SpinballROM& rom, const Uint32 offset, const std::size_t length)
		{
			const std::size_t rom_size = rom.m_buffer.size();
			const std::size_t start = static_cast<std::size_t>(offset);
			return start <= rom_size && length <= (rom_size - start);
		}
	}

	Level Level::LoadFromROM(const rom::SpinballROM& target_rom, int level_index)
	{
		Level new_level;
		new_level.m_level_index = level_index;

		if (level_index < 0 || level_index >= level_count)
		{
			std::cerr << "Cannot load level: invalid level index " << level_index << '\n';
			new_level.m_level_name = "Invalid level";
			return new_level;
		}

		if (target_rom.m_buffer.empty())
		{
			std::cerr << "Cannot load level " << level_index << ": ROM buffer is empty\n";
			new_level.m_level_name = "ROM not loaded";
			return new_level;
		}

		rom::LevelDataOffsets level_data_offsets{ level_index };
		new_level.m_data_offsets = level_data_offsets;

		// The table contains a big-endian pointer to the zero-terminated level name.
		if (!RangeIsValid(target_rom, level_data_offsets.level_name, sizeof(Uint32)))
		{
			std::cerr << "Cannot load level name pointer: table offset 0x"
				<< std::hex << level_data_offsets.level_name << " is outside ROM (size 0x"
				<< target_rom.m_buffer.size() << ")\n" << std::dec;
			new_level.m_level_name = "Invalid level name";
		}
		else
		{
			const rom::Ptr32 level_name_offset = target_rom.ReadUint32(level_data_offsets.level_name);
			if (!RangeIsValid(target_rom, level_name_offset, 1))
			{
				std::cerr << "Cannot load level name: pointer 0x" << std::hex
					<< level_name_offset << " is outside ROM (size 0x"
					<< target_rom.m_buffer.size() << ")\n" << std::dec;
				new_level.m_level_name = "Invalid level name";
			}
			else
			{
				const char* const level_name = reinterpret_cast<const char*>(
					target_rom.m_buffer.data() + static_cast<std::size_t>(level_name_offset));
				const std::size_t bytes_remaining = target_rom.m_buffer.size() -
					static_cast<std::size_t>(level_name_offset);
				const std::size_t search_length = std::min(bytes_remaining, max_level_name_length);
				const char* const name_end = std::find(level_name, level_name + search_length, '\0');
				new_level.m_level_name.assign(level_name, name_end);

				if (new_level.m_level_name.empty())
					new_level.m_level_name = "Unnamed level";
			}
		}

		new_level.m_ring_instances.clear();
		const std::size_t requested_ring_bytes =
			static_cast<std::size_t>(level_data_offsets.ring_instances.count) * ring_instance_size;

		if (!RangeIsValid(target_rom, level_data_offsets.ring_instances.offset, requested_ring_bytes))
		{
			std::cerr << "Skipping invalid ring table for level " << level_index
				<< ": offset=0x" << std::hex << level_data_offsets.ring_instances.offset
				<< ", count=0x" << level_data_offsets.ring_instances.count
				<< ", ROM size=0x" << target_rom.m_buffer.size() << '\n' << std::dec;
		}
		else
		{
			new_level.m_ring_instances.reserve(level_data_offsets.ring_instances.count);
			Uint32 current_table_offset = level_data_offsets.ring_instances.offset;

			for (Uint32 i = 0; i < level_data_offsets.ring_instances.count; ++i)
			{
				if (!RangeIsValid(target_rom, current_table_offset, ring_instance_size))
				{
					std::cerr << "Stopping ring loading at item " << i
						<< ": offset is outside ROM\n";
					break;
				}

				new_level.m_ring_instances.emplace_back(
					rom::RingInstance::LoadFromROM(target_rom, current_table_offset));
				current_table_offset += static_cast<Uint32>(ring_instance_size);
			}
		}

		// A bad ROM revision can contain a pointer that does not match these hardcoded tables.
		if (RangeIsValid(target_rom, new_level.m_data_offsets.collision_data_terrain, sizeof(Uint32)))
		{
			const rom::Ptr32 spline_offset =
				target_rom.ReadUint32(new_level.m_data_offsets.collision_data_terrain);
			if (RangeIsValid(target_rom, spline_offset, 2))
			{
				new_level.m_spline_culling_table = std::make_unique<rom::SplineCullingTable>(
					rom::SplineCullingTable::LoadFromROM(target_rom, spline_offset));
			}
			else
			{
				std::cerr << "Skipping invalid spline table pointer 0x" << std::hex
					<< spline_offset << '\n' << std::dec;
			}
		}

		return new_level;
	}

	rom::Ptr32 Level::SaveToROM(rom::SpinballROM& target_rom) const
	{
		if (m_tile_layers.size() >= 2 &&
			m_tile_layers[0].tile_layout && m_tile_layers[0].tileset &&
			m_tile_layers[1].tile_layout && m_tile_layers[1].tileset)
		{
			m_tile_layers[0].tile_layout->SaveToROM(target_rom, *m_tile_layers[0].tileset, target_rom.ReadUint32(m_data_offsets.background_tile_brushes), target_rom.ReadUint32(m_data_offsets.background_tile_layout));
			m_tile_layers[1].tile_layout->SaveToROM(target_rom, *m_tile_layers[1].tileset, target_rom.ReadUint32(m_data_offsets.foreground_tile_brushes), target_rom.ReadUint32(m_data_offsets.foreground_tile_layout));
		}

		if (m_spline_culling_table)
			m_spline_culling_table->SaveToROM(target_rom, target_rom.ReadUint32(m_data_offsets.collision_data_terrain));
		if (m_data_offsets.collision_tile_obj_ids.offset != 0 && m_game_object_culling_table)
		{
			const rom::Ptr32 obj_end = m_game_object_culling_table->SaveToROM(target_rom, m_data_offsets.collision_tile_obj_ids.offset);
			target_rom.WriteUint32(m_data_offsets.camera_activation_sector_anim_obj_ids, obj_end);
		}
		if (m_anim_object_culling_table)
			m_anim_object_culling_table->SaveToROM(target_rom, target_rom.ReadUint32(m_data_offsets.camera_activation_sector_anim_obj_ids));

		for (const rom::FlipperInstance& obj : m_flipper_instances)
			obj.SaveToROM(target_rom);

		for (const rom::RingInstance& obj : m_ring_instances)
			obj.SaveToROM(target_rom);

		const Uint8 num_emeralds = std::accumulate(std::begin(m_game_obj_instances), std::end(m_game_obj_instances), static_cast<Uint8>(0),
			[](const Uint8 running_val, const rom::GameObjectDefinition& game_obj)
			{
				return running_val + ((game_obj.instance_id != 0 && game_obj.type_id == GameObjectType::EMERALD) ? 1 : 0);
			});
		target_rom.WriteUint8(m_data_offsets.emerald_count, num_emeralds);

		return 0;
	}
}
