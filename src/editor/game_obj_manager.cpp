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

	std::unique_ptr<rom::GameObjectCullingTable> GameObjectManager::GenerateObjCollisionCullingTable() const
	{
		std::unique_ptr<rom::GameObjectCullingTable> new_table = std::make_unique<rom::GameObjectCullingTable>();

		for (const std::unique_ptr<UIGameObject>& ui_obj : game_objects)
		{
			if (ui_obj->obj_definition.collision_bbox_ptr == 0 || ui_obj->had_collision_sectors_on_rom == false)
			{
				continue;
			}

			std::vector<Point> grid_intersections;

			// Left bound
			const BoundingBox& spline_vector = ui_obj->obj_definition.collision->spline_vector;
			const rom::GameObjectDefinition& obj_def = ui_obj->obj_definition;
			const  Point pos{ ui_obj->obj_definition.x_pos, ui_obj->obj_definition.y_pos };
			const BoundingBox& obj_bbox
			{
				static_cast<Uint16>(ui_obj->obj_definition.x_pos + static_cast<Uint16>(spline_vector.min.x)),
				static_cast<Uint16>(ui_obj->obj_definition.y_pos + static_cast<Uint16>(spline_vector.min.y)),
				static_cast<Uint16>(ui_obj->obj_definition.x_pos + static_cast<Uint16>(spline_vector.max.x)),
				static_cast<Uint16>(ui_obj->obj_definition.y_pos + static_cast<Uint16>(spline_vector.max.y))
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
					rom::GameObjectCullingCell& target_cell = new_table->cells[cell_index];

					target_cell.obj_instance_ids.emplace_back(ui_obj->obj_definition.instance_id);
				}
			}
		}

		return std::move(new_table);
	}

	std::unique_ptr<rom::AnimatedObjectCullingTable> GameObjectManager::GenerateAnimObjCullingTable() const
	{
		std::unique_ptr<rom::AnimatedObjectCullingTable> new_table = std::make_unique<rom::AnimatedObjectCullingTable>();

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
					rom::AnimatedObjectCullingCell& target_cell = new_table->cells[cell_index];

					target_cell.obj_instance_ids.emplace_back(ui_obj->obj_definition.instance_id);
				}
			}
		}

		return std::move(new_table);
	}

	UIGameObject* GameObjectManager::DeleteGameObject(const UIGameObject& obj_to_remove)
	{
		auto erase_it = std::find_if(std::begin(game_objects), std::end(game_objects),
			[&obj_to_remove](const std::unique_ptr<UIGameObject>& game_obj)
			{
				return game_obj->obj_definition.instance_id == obj_to_remove.obj_definition.instance_id;
			});

		if (erase_it != std::end(game_objects))
		{
			(*erase_it)->obj_definition.instance_id = 0;
			return erase_it->get();
		}

		return nullptr;
	}

	UIGameObject* GameObjectManager::DuplicateGameObject(const UIGameObject& obj_to_duplicate, const rom::LevelDataOffsets& level_data)
	{
		std::unique_ptr<UIGameObject> new_obj = std::make_unique<UIGameObject>(obj_to_duplicate);
		
		constexpr Uint8 min_id = 0x01;
		constexpr Uint8 max_id = 0xFF;
		Uint8 previous_id = 0;
		auto empty_slot_it = std::find_if(std::begin(game_objects), std::end(game_objects),
			[&previous_id](const std::unique_ptr<UIGameObject>& game_obj)
			{
				++previous_id;
				return game_obj->obj_definition.instance_id == 0;
			});

		if( empty_slot_it != std::end(game_objects))
		{
			new_obj->obj_definition.rom_data = (*empty_slot_it)->obj_definition.rom_data;
			new_obj->obj_definition.instance_id = previous_id;
			new_obj->had_collision_sectors_on_rom = true;
			new_obj->had_culling_sectors_on_rom = true;
			(*empty_slot_it) = std::move(new_obj);
			return (*empty_slot_it).get();
		}

		return nullptr;
	}

}