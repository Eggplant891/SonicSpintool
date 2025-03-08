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
	struct AnimatedObjectCullingCell
	{
		BoundingBox bbox;
		Ptr32 jump_offset;
		std::vector<Uint8> obj_ids;
	};

	struct AnimatedObjectCullingTable
	{
		static constexpr BoundingBox cell_dimensions{ 0, 0, 0x168, 0x108 };
		static constexpr Point grid_dimensions{ 6,8 };
		static constexpr Uint32 cells_count = grid_dimensions.x * grid_dimensions.y;

		std::array<AnimatedObjectCullingCell, cells_count> cells;

		static AnimatedObjectCullingTable LoadFromROM(const SpinballROM& rom, Ptr32 offset);
		void SaveToROM(SpinballROM& rom, Ptr32 offset);
	};
}