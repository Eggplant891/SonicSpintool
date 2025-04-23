#include "rom/culling_tables/spline_culling_table.h"

#include "rom/spinball_rom.h"
#include <numeric>

namespace spintool::rom
{
	bool CollisionSpline::IsRadial() const
	{
		return (object_type_flags & 0x8000) == 0x8000;
	};

	bool CollisionSpline::IsWalkable() const
	{
		return (object_type_flags & 0x0080) == 0x0080;
	}

	bool CollisionSpline::IsSlippery() const
	{
		return (object_type_flags & 0x0100) == 0x0100;
	}

	bool CollisionSpline::IsTeleporter() const
	{
		return (object_type_flags & 0x1000) == 0x1000;
	}

	bool CollisionSpline::IsBumper() const
	{
		return (object_type_flags & 0x4000) == 0x4000;
	}

	bool CollisionSpline::IsRing() const
	{
		return (object_type_flags & 0x2000) == 0x2000;
	}

	bool CollisionSpline::IsUnknown() const
	{
		return !IsRadial() && !IsTeleporter() && !IsBumper() && !IsRing() && ((object_type_flags & 0x0FFF) != (object_type_flags & 0xFFFF));
	}

	bool CollisionSpline::operator==(const CollisionSpline& rhs) const
	{
		return spline_vector == rhs.spline_vector
			&& object_type_flags == rhs.object_type_flags
			&& instance_id_binding == rhs.instance_id_binding;
	}

	CollisionSpline CollisionSpline::LoadFromROM(const SpinballROM& rom, Ptr32 offset)
	{
		CollisionSpline new_spline;

		Ptr32 current_offset = offset;

		new_spline.spline_vector.min.x = rom.ReadUint16(current_offset);
		current_offset += 2;
		new_spline.spline_vector.min.y = rom.ReadUint16(current_offset);
		current_offset += 2;
		new_spline.spline_vector.max.x = rom.ReadUint16(current_offset);
		current_offset += 2;
		new_spline.spline_vector.max.y = rom.ReadUint16(current_offset);
		current_offset += 2;

		new_spline.object_type_flags = rom.ReadUint16(current_offset);
		current_offset += 2;
		new_spline.instance_id_binding = rom.ReadUint16(current_offset);
		current_offset += 2;

		new_spline.rom_data.SetROMData(offset, current_offset);

		return new_spline;
	}

	Ptr32 CollisionSpline::SaveToROM(SpinballROM& rom, Ptr32 offset) const
	{
		Ptr32 current_offset = offset;

		current_offset = rom.WriteUint16(current_offset, spline_vector.min.x);
		current_offset = rom.WriteUint16(current_offset, spline_vector.min.y);
		current_offset = rom.WriteUint16(current_offset, spline_vector.max.x);
		current_offset = rom.WriteUint16(current_offset, spline_vector.max.y);

		current_offset = rom.WriteUint16(current_offset, object_type_flags);
		current_offset = rom.WriteUint16(current_offset, instance_id_binding);

		return current_offset;
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
				new_table.cells[i].splines[s] = CollisionSpline::LoadFromROM(rom, data_start_offset + short_offset);
			}
		}

		return new_table;
	}

	Ptr32 SplineCullingTable::SaveToROM(SpinballROM& rom, Ptr32 offset) const
	{
		Ptr32 jump_table_offset = offset;
		Ptr32 data_offset = jump_table_offset + static_cast<Uint32>(cells_count) * 2;

		for (const SplineCullingCell& cell : cells)
		{
			jump_table_offset = rom.WriteUint16(jump_table_offset, (data_offset - offset) / 2);
			data_offset = rom.WriteUint16(data_offset, static_cast<Uint16>(cell.splines.size()));
			for (const CollisionSpline& spline : cell.splines)
			{
				data_offset = spline.SaveToROM(rom, data_offset);
			}
		}
		data_offset = rom.WriteUint16(data_offset, (data_offset - jump_table_offset) / 2);
		return data_offset;
	}

	Uint32 SplineCullingTable::CalculateTableSize() const
	{
		return static_cast<Uint32>(std::size(cells) * 2) + std::accumulate(std::begin(cells), std::end(cells), 0,
			[](Uint32 current_count, const SplineCullingCell& cell)
			{
				return current_count + (static_cast<Uint32>(cell.splines.size()) * CollisionSpline::size_on_rom);
			});
	}

}