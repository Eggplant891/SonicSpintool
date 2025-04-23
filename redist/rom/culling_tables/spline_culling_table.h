#pragma once

#include "types/bounding_box.h"
#include "types/rom_ptr.h"
#include "rom/rom_data.h"

#include "SDL3/SDL_stdinc.h"
#include <array>
#include <vector>

namespace spintool::rom
{
	class SpinballROM;
}

namespace spintool::rom
{
	enum class CollisionObjectType
	{
		Spline,
		Radial
	};

	struct CollisionSpline
	{
		static constexpr Uint32 size_on_rom = 12;
		
		ROMData rom_data;

		BoundingBox spline_vector;
		Uint16 object_type_flags = 0;
		Uint16 instance_id_binding = 0;

		bool IsRadial() const;
		bool IsTeleporter() const;
		bool IsWalkable() const;
		bool IsSlippery() const;
		bool IsBumper() const;
		bool IsRing() const;
		bool IsUnknown() const;

		bool operator==(const CollisionSpline& rhs) const;

		static CollisionSpline LoadFromROM(const SpinballROM& rom, Ptr32 offset);
		Ptr32 SaveToROM(SpinballROM& rom, Ptr32 offset) const;
	};

	struct SplineCullingCell
	{
		Ptr32 jump_offset = 0;
		std::vector<CollisionSpline> splines;
	};

	struct SplineCullingTable
	{
		static constexpr Point cell_dimensions{ 128, 128 };
		static constexpr Point grid_dimensions{ 16, 16 };
		static constexpr Uint32 cells_count = grid_dimensions.x * grid_dimensions.y;

		std::array<SplineCullingCell, cells_count> cells;

		Uint32 CalculateTableSize() const;

		static SplineCullingTable LoadFromROM(const SpinballROM& rom, Ptr32 offset);
		Ptr32 SaveToROM(SpinballROM& rom, Ptr32 offset) const;
	};
}