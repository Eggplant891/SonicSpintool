#pragma once

#include "types/rom_ptr.h"
#include "types/bounding_box.h"

#include "SDL3/SDL_stdinc.h"

namespace spintool
{
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

	class AnimationObject
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

	//private:
		Point m_origin;
		BoundingBox m_bbox;

		Uint8 m_type_id = 0;
		Uint8 m_instance_id = 0;
		bool m_flip_x = 0;
		bool m_flip_y = 0;
		AnimationObject m_animation;
	};

	class GameObjectManager
	{

	};
}