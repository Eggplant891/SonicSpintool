#pragma once

#include "rom/culling_tables/spline_culling_table.h"

namespace spintool
{
	class SplineManager
	{
	public:
		void LoadFromSplineCullingTable(const rom::SplineCullingTable& spline_table);
		rom::SplineCullingTable GenerateSplineCullingTable() const;

		std::vector<rom::CollisionSpline> splines;
	};
}