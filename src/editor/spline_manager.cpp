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

	std::unique_ptr<rom::SplineCullingTable> SplineManager::GenerateSplineCullingTable() const
	{
		std::unique_ptr<rom::SplineCullingTable> new_table = std::make_unique<rom::SplineCullingTable>();
		for (const rom::CollisionSpline& spline : splines)
		{
			auto test_cell_and_add = [&spline, &new_table](const float x, const float y)
				{
					const Uint32 cell_x = static_cast<int>(x) / rom::SplineCullingTable::cell_dimensions.x;
					const Uint32 cell_y = static_cast<int>(y) / rom::SplineCullingTable::cell_dimensions.y;
					const size_t cell_index = (cell_y * rom::SplineCullingTable::grid_dimensions.x) + cell_x;
					if (cell_x == rom::SplineCullingTable::grid_dimensions.x || cell_y == rom::SplineCullingTable::grid_dimensions.y || cell_index >= new_table->cells.size())
					{
						//std::cout << "Discarded spline in cell [" << cell_x << "," << cell_y << "]" << std::endl;
						return;
					}

					rom::SplineCullingCell& target_cell = new_table->cells[cell_index];
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
				const auto x = spline.spline_vector.min.x;
				const auto y = spline.spline_vector.min.y;
				const auto vector_x = spline.spline_vector.max.x - spline.spline_vector.min.x;
				const auto vector_y = spline.spline_vector.max.y - spline.spline_vector.min.y;
				const int num_tests = std::max(1, static_cast<int>(std::sqrtf(std::powf(static_cast<float>(vector_x), 2) + std::powf(static_cast<float>(vector_y), 2))));
				for (int i = 0; num_tests != 0 && i <= num_tests; ++i)
				{
					const float delta = static_cast<float>(i) / static_cast<float>(num_tests);
					test_cell_and_add(x + (vector_x * delta), y + (vector_y * delta));
				}
			}
		}
		return std::move(new_table);
	}
}