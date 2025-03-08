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
	struct GameObjectCullingCell
	{
		BoundingBox bbox;
		Ptr32 jump_offset;
		std::vector<Uint16> obj_ids;
	};

	struct GameObjectCullingTable
	{
		static constexpr BoundingBox cell_dimensions{ 0, 0, 128, 128 };
		static constexpr Point grid_dimensions{ 16,16 };
		static constexpr Uint32 cells_count = grid_dimensions.x * grid_dimensions.y;

		std::array<GameObjectCullingCell, cells_count> cells;

		static GameObjectCullingTable LoadFromROM(const SpinballROM& rom, Ptr32 offset);
		void SaveToROM(SpinballROM& rom, Ptr32 offset);
	};
}