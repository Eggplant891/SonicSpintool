#include "rom/culling_tables/game_obj_collision_culling_table.h"
#include "rom/spinball_rom.h"

namespace spintool::rom
{

	GameObjectCullingTable GameObjectCullingTable::LoadFromROM(const SpinballROM& rom, Ptr32 offset)
	{
		GameObjectCullingTable new_table;

		Ptr32 next_offset = offset;

		for (GameObjectCullingCell& cell : new_table.cells)
		{
			cell.jump_offset = rom.ReadUint16(offset);
			offset += 2;
		}

		for (int i = 0; i < 0xFF; ++i)
		{
			const Uint16 start_offset = offset + new_table.cells[i].jump_offset;
			const Uint16 end_offset = offset + new_table.cells[i+1].jump_offset;

			GameObjectCullingCell& cell = new_table.cells[i];

			cell.bbox.min.x = (i % grid_dimensions.x) * cell_dimensions.Width();
			cell.bbox.min.y = (i / grid_dimensions.x) * cell_dimensions.Height();
			cell.bbox.max.x = cell.bbox.min.x + cell_dimensions.Width();
			cell.bbox.max.y = cell.bbox.min.y + cell_dimensions.Height();

			for (Uint16 current_offset = start_offset; current_offset <= end_offset-1; current_offset += 2)
			{
				new_table.cells[i].obj_ids.emplace_back(rom.ReadUint16(current_offset));
			}
		}

		return new_table;
	}

	void GameObjectCullingTable::SaveToROM(SpinballROM& rom, Ptr32 offset)
	{
		Ptr32 jump_table_offset = offset;
		Ptr32 data_offset = offset + static_cast<Uint32>(cells.size()) * 2;

		for (GameObjectCullingCell& cell : cells)
		{
			jump_table_offset = rom.WriteUint16(jump_table_offset, cell.jump_offset);
			for (Uint16 game_obj_id : cell.obj_ids)
			{
				data_offset += 2;
			}
		}

		for (int i = 0; i < 0xFF; ++i)
		{
			const Uint16 start_offset = offset + cells[i].jump_offset;
			const Uint16 end_offset = offset + cells[i + 1].jump_offset;

			for (Uint16 current_offset = start_offset; start_offset <= end_offset - 1; current_offset += 2)
			{
				cells[i].obj_ids.emplace_back(rom.ReadUint16(current_offset));
			}
		}
	}

}