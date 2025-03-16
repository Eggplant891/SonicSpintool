#include "rom/culling_tables/game_obj_collision_culling_table.h"
#include "rom/spinball_rom.h"

namespace spintool::rom
{

	GameObjectCullingTable GameObjectCullingTable::LoadFromROM(const SpinballROM& rom, const Ptr32 offset)
	{
		GameObjectCullingTable new_table;

		Ptr32 next_cell_offset = offset;

		for (GameObjectCullingCell& cell : new_table.cells)
		{
			cell.jump_offset = rom.ReadUint16(next_cell_offset);
			next_cell_offset += 2;
		}

		for (int i = 0; i < cells_count-1; ++i)
		{
			Uint32 cell_offset = offset + (new_table.cells[i].jump_offset * 2);
			const Uint16 num_objs = rom.ReadUint16(cell_offset);
			cell_offset += 2;

			GameObjectCullingCell& cell = new_table.cells[i];

			cell.bbox.min.x = (i % grid_dimensions.x) * cell_dimensions.Width();
			cell.bbox.min.y = (i / grid_dimensions.x) * cell_dimensions.Height();
			cell.bbox.max.x = cell.bbox.min.x + cell_dimensions.Width();
			cell.bbox.max.y = cell.bbox.min.y + cell_dimensions.Height();

			for (Uint32 current_offset = cell_offset; current_offset < cell_offset + (num_objs*2); current_offset += 2)
			{
				new_table.cells[i].obj_ids.emplace_back(rom.ReadUint16(current_offset));
			}
		}

		return new_table;
	}

	void GameObjectCullingTable::SaveToROM(SpinballROM& rom, Ptr32 offset)
	{
		Ptr32 jump_table_offset = offset;
		Ptr32 data_offset = static_cast<Uint32>(cells_count) * 2;

		for (GameObjectCullingCell& cell : cells)
		{
			jump_table_offset = rom.WriteUint16(jump_table_offset, data_offset / 2);
			data_offset = rom.WriteUint16(jump_table_offset, static_cast<Uint16>(cell.obj_ids.size()));
			for (const Uint16 obj_id : cell.obj_ids)
			{
				data_offset = rom.WriteUint16(data_offset, obj_id);
			}
		}
		rom.WriteUint16(data_offset, (data_offset - jump_table_offset) / 2);
	}

}