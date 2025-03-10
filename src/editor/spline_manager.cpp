#include "editor/spline_manager.h"
#include <iostream>
#include <cassert>

namespace spintool
{
	void SplineManager::LoadFromSplineCullingTable(const rom::SplineCullingTable& spline_table)
	{
		splines.clear();
		for (const rom::SplineCullingCell& cell : spline_table.cells)
		{
			for (const rom::CollisionSpline& spline : cell.splines)
			{
				const bool found_spline = std::any_of(std::begin(splines), std::end(splines),
					[&spline](const rom::CollisionSpline& _spline)
					{
						return _spline == spline;
					});

				if (found_spline == false)
				{
					splines.emplace_back(spline);
				}
			}
		}
	}

	rom::SplineCullingTable SplineManager::GenerateSplineCullingTable() const
	{
		rom::SplineCullingTable new_table;
		for (const rom::CollisionSpline& spline : splines)
		{
			auto test_cell_and_add = [&spline, &new_table](const float x, const float y)
				{
					const Uint32 cell_x = static_cast<int>(x) / rom::SplineCullingTable::cell_dimensions.x;
					const Uint32 cell_y = static_cast<int>(y) / rom::SplineCullingTable::cell_dimensions.y;
					const size_t cell_index = (cell_y * rom::SplineCullingTable::grid_dimensions.x) + cell_x;
					if (cell_x == rom::SplineCullingTable::grid_dimensions.x || cell_y == rom::SplineCullingTable::grid_dimensions.y || cell_index >= new_table.cells.size())
					{
						//std::cout << "Discarded spline in cell [" << cell_x << "," << cell_y << "]" << std::endl;
						return;
					}

					rom::SplineCullingCell& target_cell = new_table.cells[cell_index];
					if (std::none_of(std::begin(target_cell.splines), std::end(target_cell.splines), [&spline](const rom::CollisionSpline& _spline)
						{
							return _spline == spline;
						}))
					{
						target_cell.splines.emplace_back(spline);
					}
				};

			if (spline.IsRadial() || spline.IsTeleporter())
			{
				const float radius = static_cast<float>(spline.spline_vector.max.x);
				auto x = spline.spline_vector.min.x;
				auto y = spline.spline_vector.min.y;

				test_cell_and_add(static_cast<float>(x), static_cast<float>(y));
				test_cell_and_add(static_cast<float>(x) + radius, static_cast<float>(y));
				test_cell_and_add(static_cast<float>(x), static_cast<float>(y) + radius);
				test_cell_and_add(static_cast<float>(x) - radius, static_cast<float>(y));
				test_cell_and_add(static_cast<float>(x), static_cast<float>(y) - radius);

				test_cell_and_add(static_cast<float>(x) + radius, static_cast<float>(y) + radius);
				test_cell_and_add(static_cast<float>(x) - radius, static_cast<float>(y) - radius);
				test_cell_and_add(static_cast<float>(x) - radius, static_cast<float>(y) + radius);
				test_cell_and_add(static_cast<float>(x) + radius, static_cast<float>(y) - radius);

			}
			else
			{
				assert(spline.IsUnknown() == false);
				const int num_tests = std::max(1, static_cast<int>(std::sqrtf(std::powf(static_cast<float>(spline.spline_vector.Width() + spline.spline_vector.Height()), 2)) / 8.0f));
				for (int i = 0; num_tests != 0 && i <= num_tests; ++i)
				{
					const float delta = static_cast<float>(i) / static_cast<float>(num_tests);
					auto x = spline.spline_vector.MinOrdered().x;
					auto y = spline.spline_vector.MinOrdered().y;
					test_cell_and_add(x + (spline.spline_vector.Width() * delta), y + (spline.spline_vector.Height() * delta));
				}
			}
		}
		return new_table;
	}
}