#pragma once

#include "nlohmann/json_fwd.hpp"

#include <string>
#include <memory>

namespace spintool::metadata
{
	struct LevelMetadata;
}

namespace spintool::rom
{
	struct SplineCullingTable;
	struct GameObjectCullingTable;
	struct AnimatedObjectCullingTable;
	struct TileLayout;
	struct TileSet;
}

namespace spintool::editor
{
	class Level
	{
	public:
		std::string level_filename;
		int m_level_index = -1;
		// std::vector<editor::TileLayer> m_tile_layers;
		// std::vector<editor::FlipperInstance> m_flipper_instances;
		// std::vector<editor::RingInstance> m_ring_instances;
		std::vector<metadata::LevelMetadata> m_level_metadata;
		// std::vector<editor::GameObjectInstance> m_game_obj_instances;
		// std::vector<editor::AnimObjectDefinition> m_anim_obj_instances;

		std::unique_ptr<rom::GameObjectCullingTable> m_game_object_culling_table;
		std::unique_ptr<rom::AnimatedObjectCullingTable> m_anim_object_culling_table;
		std::unique_ptr<rom::SplineCullingTable> m_spline_culling_table;
	};
}