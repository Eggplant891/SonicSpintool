#pragma once

#include "rom/animated_object.h"
#include "rom/game_objects/game_object_flipper.h"
#include "rom/game_objects/game_object_ring.h"
#include "rom/game_objects/game_object_definition.h"
#include "rom/tile_layout.h"
#include "rom/tileset.h"

#include "editor/game_obj_manager.h"

#include <string>
#include <vector>
#include <memory>

namespace spintool
{
	class Level
	{
	public:

	//private:
		std::string m_level_name;

		std::shared_ptr<const rom::TileSet> m_tileset;
		std::shared_ptr<rom::TileLayout> m_tile_layout;
		std::vector<rom::FlipperInstance> m_flipper_instances;
		std::vector<rom::RingInstance> m_ring_instances;
		std::vector<rom::GameObjectDefinition> m_game_obj_instances;
		//std::vector<GameObject> m_game_obj_instances;
		std::vector<rom::AnimObjectDefinition> m_anim_obj_instances;
	};

	class LevelManager
	{
		std::vector<Level> m_levels;
	};
}