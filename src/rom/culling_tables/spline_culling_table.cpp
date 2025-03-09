#include "rom/culling_tables/spline_culling_table.h"

#include "rom/spinball_rom.h"

namespace spintool::rom
{
	bool CollisionSpline::IsRadial() const
	{
		return (object_type_flags & 0x8000) == 0x8000;
	};

	bool CollisionSpline::IsTeleporter() const
	{
		return (object_type_flags & 0x1000) == 0x1000;
	}

	bool CollisionSpline::IsRecognisedButUnknown() const
	{
		return (object_type_flags & 0x4000) == 0x4000;
	}

	bool CollisionSpline::IsUnknown() const
	{
		return !IsRadial() && !IsTeleporter() && !IsRecognisedButUnknown() && ((object_type_flags & 0x0FFF) != (object_type_flags & 0xFFFF));
	}

	bool CollisionSpline::operator==(const CollisionSpline& rhs) const
	{
		return spline_vector == rhs.spline_vector
			&& object_type_flags == rhs.object_type_flags
			&& extra_info == rhs.extra_info;
	}

	SplineCullingTable SplineCullingTable::LoadFromROM(const SpinballROM& rom, const Ptr32 offset)
	{
		rom; offset;
		SplineCullingTable new_table;

		Ptr32 next_offset = offset;

		for (SplineCullingCell& cell : new_table.cells)
		{
			cell.jump_offset = rom.ReadUint16(next_offset);
			next_offset += 2;
		}

		for (int i = 0; i < cells_count - 1; ++i)
		{
			const Uint32 start_offset = offset + (new_table.cells[i].jump_offset * 2);
			const Uint32 data_start_offset = start_offset + 2;
			const Uint32 end_offset = offset + (new_table.cells[i + 1].jump_offset * 2);

			SplineCullingCell& cell = new_table.cells[i];
			const Uint16 num_objects = rom.ReadUint16(start_offset);
			new_table.cells[i].splines.resize(num_objects);

			for (Uint16 s = 0; s < num_objects; ++s)
			{
				const rom::Ptr32 short_offset = s * CollisionSpline::size_on_rom;
				CollisionSpline& spline = new_table.cells[i].splines[s];

				spline.spline_vector.min.x = rom.ReadUint16(data_start_offset + short_offset + 0);
				spline.spline_vector.min.y = rom.ReadUint16(data_start_offset + short_offset + 2);
				spline.spline_vector.max.x = rom.ReadUint16(data_start_offset + short_offset + 4);
				spline.spline_vector.max.y = rom.ReadUint16(data_start_offset + short_offset + 6);

				spline.object_type_flags = rom.ReadUint16(data_start_offset + short_offset + 8);
				spline.extra_info = rom.ReadUint16(data_start_offset + short_offset + 10);
			}
		}

		return new_table;
	}

	void SplineCullingTable::SaveToROM(SpinballROM& rom, Ptr32 offset)
	{
		Ptr32 jump_table_offset = offset;
		Ptr32 data_offset = jump_table_offset + static_cast<Uint32>(cells_count) * 2;

		for (SplineCullingCell& cell : cells)
		{
			jump_table_offset = rom.WriteUint16(jump_table_offset, (data_offset - offset) / 2);
			data_offset = rom.WriteUint16(data_offset, static_cast<Uint16>(cell.splines.size()));
			for (const CollisionSpline& spline : cell.splines)
			{
				data_offset = rom.WriteUint16(data_offset, spline.spline_vector.min.x);
				data_offset = rom.WriteUint16(data_offset, spline.spline_vector.min.y);
				data_offset = rom.WriteUint16(data_offset, spline.spline_vector.max.x);
				data_offset = rom.WriteUint16(data_offset, spline.spline_vector.max.y);

				data_offset = rom.WriteUint16(data_offset, spline.object_type_flags);
				data_offset = rom.WriteUint16(data_offset, spline.extra_info);
			}
		}
		rom.WriteUint16(data_offset, (data_offset - jump_table_offset) / 2);
	}

}