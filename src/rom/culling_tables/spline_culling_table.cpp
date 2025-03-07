#include "rom/culling_tables/spline_culling_table.h"

namespace spintool::rom
{

	SplineCullingTable SplineCullingTable::LoadFromROM(const SpinballROM& rom, Ptr32 offset)
	{
		rom; offset;
		SplineCullingTable table;

		return table;
	}

	void SplineCullingTable::SaveToROM(SpinballROM& rom, Ptr32 offset)
	{
		rom; offset;
	}

}