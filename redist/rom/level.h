#pragma once

#include "rom/animated_object.h"
#include "rom/game_objects/game_object_flipper.h"
#include "rom/game_objects/game_object_ring.h"
#include "rom/game_objects/game_object_definition.h"
#include "rom/tile_layout.h"
#include "rom/palette.h"

#include "rom/rom_asset_definitions.h"
#include "types/rom_ptr.h"

#include <string>
#include <vector>
#include <memory>

namespace spintool::rom
{
	class SpinballROM;
	struct AnimatedObjectCullingTable;
	struct GameObjectCullingTable;
	struct SplineCullingTable;
}

namespace spintool::rom
{
	struct TileLayer
	{
		std::shared_ptr<const rom::TileSet> tileset;
		std::shared_ptr<rom::TileLayout> tile_layout;
		rom::PaletteSet palette_set;
	};

	class Level
	{
	public:
		//private:
		rom::LevelDataOffsets m_data_offsets;
		std::string m_level_name;
		int m_level_index = -1;
		std::vector<rom::TileLayer> m_tile_layers;
		std::vector<rom::FlipperInstance> m_flipper_instances;
		std::vector<rom::RingInstance> m_ring_instances;
		std::vector<rom::GameObjectDefinition> m_game_obj_instances;
		std::vector<rom::AnimObjectDefinition> m_anim_obj_instances;

		std::unique_ptr<rom::GameObjectCullingTable> m_game_object_culling_table;
		std::unique_ptr<rom::AnimatedObjectCullingTable> m_anim_object_culling_table;
		std::unique_ptr<rom::SplineCullingTable> m_spline_culling_table;

		static Level LoadFromROM(const rom::SpinballROM& rom, int level_index);
		rom::Ptr32 SaveToROM(rom::SpinballROM& rom) const;
	};
}