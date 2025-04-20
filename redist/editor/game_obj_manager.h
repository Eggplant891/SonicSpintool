#pragma once

#include "rom/culling_tables/game_obj_collision_culling_table.h"
#include "rom/culling_tables/animated_object_culling_table.h"
#include "rom/game_objects/game_object_definition.h"

#include "types/rom_ptr.h"
#include "types/bounding_box.h"

#include "SDL3/SDL_stdinc.h"
#include "imgui.h"

#include <memory>

namespace spintool::rom
{
	struct LevelDataOffsets;
}

namespace spintool
{
	struct UISpriteTexture;

	struct UIGameObject
	{
		rom::GameObjectDefinition obj_definition;
		ImVec2 GetSpriteDrawPos() const
		{
			return ImVec2{ obj_definition.x_pos + sprite_pos_offset.x, obj_definition.y_pos + sprite_pos_offset.y };
		}
		ImVec2 sprite_pos_offset = { 0,0 };
		ImVec2 dimensions = { 0,0 };

		int sprite_table_address = 0;
		int palette_index = 0;

		std::shared_ptr<UISpriteTexture> ui_sprite;

		bool had_collision_sectors_on_rom = false;
		bool had_culling_sectors_on_rom = false;
	};

	enum class GameObjectType
	{
		ANY,
		RING,
		FLIPPER,
		BBOX_COLLIDABLE,
		RADIAL_COLLIDABLE,
		SPLINE_COLLIDABLE,
		SCENERY,
		COUNT
	};

	class SpriteTable
	{

	};

	class AnimationData
	{
		rom::Ptr32 sprite_table = 0;
		rom::Ptr32 starting_animation = 0;
		Uint8 palette_index = 0;
		Uint8 frame_time_ticks = 0;
	};

	class GameObject
	{
	public:
		void MoveObjectTo(int x, int y);
		void MoveObjectRelative(int x, int y);

		Point m_origin;
		BoundingBox m_bbox;

		Uint8 m_type_id = 0;
		Uint8 m_instance_id = 0;
		bool m_flip_x = 0;
		bool m_flip_y = 0;
		AnimationData m_animation;
	};

	class GameObjectManager
	{
	public:
		std::unique_ptr<rom::GameObjectCullingTable> GenerateObjCollisionCullingTable() const;
		std::unique_ptr<rom::AnimatedObjectCullingTable> GenerateAnimObjCullingTable() const;

		UIGameObject* DeleteGameObject(const UIGameObject& obj_to_remove);
		UIGameObject* DuplicateGameObject(const UIGameObject& obj_to_duplicate, const rom::LevelDataOffsets& level_data);

		//std::vector<GameObject> game_objects;
		std::vector<std::unique_ptr<UIGameObject>> game_objects;
	};
}