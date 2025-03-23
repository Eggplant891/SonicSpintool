#include "editor/game_obj_manager.h"

#include "rom/culling_tables/game_obj_collision_culling_table.h"
#include "ui/ui_tile_layout_viewer.h"
#include "types/bounding_box.h"

namespace spintool
{

	void GameObject::MoveObjectTo(int x, int y)
	{
		m_bbox.max.x = x - m_origin.x + m_bbox.Width();
		m_bbox.min.x = x - m_origin.x;

		m_bbox.max.y = y - m_origin.x + m_bbox.Height();
		m_bbox.min.y = y - m_origin.x;
	}

	void GameObject::MoveObjectRelative(int x, int y)
	{

	}

	rom::GameObjectCullingTable GameObjectManager::GenerateObjCollisionCullingTable() const
	{
		rom::GameObjectCullingTable new_table;

		for (const std::unique_ptr<UIGameObject>& ui_obj : game_objects)
		{
			if (ui_obj->had_collision_sectors_on_rom == false)
			{
				continue;
			}

			std::vector<Point> grid_intersections;
			// Left bound
			const BoundingBox& obj_bbox =
			{
				static_cast<int>(ui_obj->GetSpriteDrawPos().x),
				static_cast<int>(ui_obj->GetSpriteDrawPos().y),
				static_cast<int>(ui_obj->GetSpriteDrawPos().x + ui_obj->dimensions.x),
				static_cast<int>(ui_obj->GetSpriteDrawPos().y + ui_obj->dimensions.y)
			};
			BoundingBox cell_range;
			cell_range.min.x = std::clamp(obj_bbox.MinOrdered().x / rom::GameObjectCullingTable::cell_dimensions.Width(), 0,  rom::GameObjectCullingTable::grid_dimensions.x-1);
			cell_range.min.y = std::clamp(obj_bbox.MinOrdered().y / rom::GameObjectCullingTable::cell_dimensions.Height(), 0, rom::GameObjectCullingTable::grid_dimensions.y-1);
			cell_range.max.x = std::clamp(obj_bbox.MaxOrdered().x / rom::GameObjectCullingTable::cell_dimensions.Width(), 0,  rom::GameObjectCullingTable::grid_dimensions.x-1);
			cell_range.max.y = std::clamp(obj_bbox.MaxOrdered().y / rom::GameObjectCullingTable::cell_dimensions.Height(), 0, rom::GameObjectCullingTable::grid_dimensions.y-1);

			for (int cell_x = cell_range.MinOrdered().x; cell_x <= cell_range.MaxOrdered().x; ++cell_x)
			{
				for (int cell_y = cell_range.MinOrdered().y; cell_y <= cell_range.MaxOrdered().y; ++cell_y)
				{
					const size_t cell_index = (cell_y * rom::GameObjectCullingTable::grid_dimensions.x) + cell_x;
					rom::GameObjectCullingCell& target_cell = new_table.cells[cell_index];

					target_cell.obj_instance_ids.emplace_back(ui_obj->obj_definition.instance_id);
				}
			}
		}

		return new_table;
	}

	rom::AnimatedObjectCullingTable GameObjectManager::GenerateAnimObjCullingTable() const
	{
		rom::AnimatedObjectCullingTable new_table;

		for (const std::unique_ptr<UIGameObject>& ui_obj : game_objects)
		{
			if (ui_obj->had_culling_sectors_on_rom == false)
			{
				continue;
			}

			std::vector<Point> grid_intersections;
			// Left bound
			const BoundingBox& obj_bbox =
			{
				static_cast<int>(ui_obj->GetSpriteDrawPos().x),
				static_cast<int>(ui_obj->GetSpriteDrawPos().y),
				static_cast<int>(ui_obj->GetSpriteDrawPos().x) + ui_obj->obj_definition.collision_width,
				static_cast<int>(ui_obj->GetSpriteDrawPos().y) + ui_obj->obj_definition.collision_height
			}; 

			BoundingBox cell_range;
			cell_range.min.x = std::clamp(obj_bbox.MinOrdered().x / rom::AnimatedObjectCullingTable::cell_dimensions.Width(), 0,  rom::AnimatedObjectCullingTable::grid_dimensions.x - 1);
			cell_range.min.y = std::clamp(obj_bbox.MinOrdered().y / rom::AnimatedObjectCullingTable::cell_dimensions.Height(), 0, rom::AnimatedObjectCullingTable::grid_dimensions.y - 1);
			cell_range.max.x = std::clamp(obj_bbox.MaxOrdered().x / rom::AnimatedObjectCullingTable::cell_dimensions.Width(), 0,  rom::AnimatedObjectCullingTable::grid_dimensions.x - 1);
			cell_range.max.y = std::clamp(obj_bbox.MaxOrdered().y / rom::AnimatedObjectCullingTable::cell_dimensions.Height(), 0, rom::AnimatedObjectCullingTable::grid_dimensions.y - 1);

			for (int cell_x = cell_range.MinOrdered().x; cell_x <= cell_range.MaxOrdered().x; ++cell_x)
			{
				for (int cell_y = cell_range.MinOrdered().y; cell_y <= cell_range.MaxOrdered().y; ++cell_y)
				{
					const size_t cell_index = (cell_y * rom::AnimatedObjectCullingTable::grid_dimensions.x) + cell_x;
					rom::AnimatedObjectCullingCell& target_cell = new_table.cells[cell_index];

					target_cell.obj_instance_ids.emplace_back(ui_obj->obj_definition.instance_id);
				}
			}
		}

		return new_table;
	}

}