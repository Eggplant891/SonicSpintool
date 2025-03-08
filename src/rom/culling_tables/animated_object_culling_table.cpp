#include "rom/culling_tables/animated_object_culling_table.h"

#include "rom/spinball_rom.h"

namespace spintool
{

	spintool::rom::AnimatedObjectCullingTable rom::AnimatedObjectCullingTable::LoadFromROM(const SpinballROM& rom, const Ptr32 offset)
	{
		AnimatedObjectCullingTable new_table;

		Ptr32 next_offset = offset;

		for (AnimatedObjectCullingCell& cell : new_table.cells)
		{
			cell.jump_offset = rom.ReadUint8(next_offset);
			next_offset += 1;
		}

		for (int i = 0; i < cells_count-1; ++i)
		{
			const Uint32 start_offset = offset + new_table.cells[i].jump_offset;
			const Uint32 end_offset = offset + new_table.cells[i + 1].jump_offset;

			AnimatedObjectCullingCell& cell = new_table.cells[i];

			cell.bbox.min.x = (i % grid_dimensions.x) * cell_dimensions.Width();
			cell.bbox.min.y = (i / grid_dimensions.x) * cell_dimensions.Height();
			cell.bbox.max.x = cell.bbox.min.x + cell_dimensions.Width();
			cell.bbox.max.y = cell.bbox.min.y + cell_dimensions.Height();

			for (Uint32 current_offset = start_offset; current_offset < end_offset; current_offset += 1)
			{
				new_table.cells[i].obj_ids.emplace_back(rom.ReadUint8(current_offset));
			}
		}

		return new_table;
	}

	void rom::AnimatedObjectCullingTable::SaveToROM(SpinballROM& rom, const Ptr32 offset)
	{
		Ptr32 jump_table_offset = offset;
		Ptr32 data_offset = static_cast<Uint32>(cells.size());

		for (AnimatedObjectCullingCell& cell : cells)
		{
			jump_table_offset = rom.WriteUint8(jump_table_offset, data_offset);
			for (const Uint8 obj_id : cell.obj_ids)
			{
				data_offset = rom.WriteUint8(data_offset, obj_id);
			}

			rom.WriteUint8(data_offset, data_offset - jump_table_offset);
		}
	}

}