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
		std::vector<Uint16> obj_ids;
	};

	struct AnimatedObjectCullingTable
	{
		static constexpr BoundingBox cell_dimensions{ 0, 0, 360, 260 };
		static constexpr Point grid_dimensions{ 16,16 };

		std::array<AnimatedObjectCullingCell, 0x100> cells;

		static AnimatedObjectCullingTable LoadFromROM(const SpinballROM& rom, Ptr32 offset);
		void SaveToROM(SpinballROM& rom, Ptr32 offset);
	};
}