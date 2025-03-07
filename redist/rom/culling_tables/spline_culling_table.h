#include "types/bounding_box.h"
#include "types/rom_ptr.h"

#include "SDL3/SDL_stdinc.h"
#include <array>
#include <vector>

namespace spintool::rom
{
	class SpinballROM;
}

namespace spintool::rom
{
	struct CollisionSpline
	{
		BoundingBox spline_vector;

	};

	struct SplineCell
	{
		Ptr32 jump_offset;
		std::vector<CollisionSpline> splines;
	};

	struct SplineCullingTable
	{
		static constexpr BoundingBox grid_dimensions{ 0, 0, 128, 128 };

		std::array<Ptr32, 0x100> jump_offsets;
		std::vector<Uint8> anim_obj_ids;

		static SplineCullingTable LoadFromROM(const SpinballROM& rom, Ptr32 offset);
		void SaveToROM(SpinballROM& rom, Ptr32 offset);
	};
}