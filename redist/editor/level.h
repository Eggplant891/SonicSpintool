#pragma once

#include "rom/animated_object.h"
#include "rom/game_objects/game_object_flipper.h"
#include "rom/game_objects/game_object_ring.h"
#include "rom/game_objects/game_object_definition.h"
#include "rom/palette.h"
#include "rom/tile_layout.h"
#include "rom/tileset.h"

#include "editor/game_obj_manager.h"
#include "types/rom_ptr.h"
#include "types/sdl_handle_defs.h"

#include <string>
#include <vector>
#include <memory>

namespace spintool::rom
{
	class SpinballROM;
}

namespace spintool
{
	struct TileLayer
	{
		std::shared_ptr<const rom::TileSet> tileset;
		std::shared_ptr<rom::TileLayout> tile_layout;
		rom::PaletteSet palette_set;
	};

	struct TileBrushPreview
	{
		SDLSurfaceHandle surface;
		SDLTextureHandle texture;

		Uint32 brush_index;
	};
}

namespace spintool::rom
{
	class Level
	{
	public:

	//private:
		std::string m_level_name;
		int level_index = -1;
		std::vector<TileLayer> m_tile_layers;

		std::vector<rom::FlipperInstance> m_flipper_instances;
		std::vector<rom::RingInstance> m_ring_instances;
		std::vector<rom::GameObjectDefinition> m_game_obj_instances;
		std::vector<rom::AnimObjectDefinition> m_anim_obj_instances;

		static Level LoadFromROM(const rom::SpinballROM& rom, int level_index);
		rom::Ptr32 SaveToROM(rom::SpinballROM& rom, int level_index) const;
	};

	class LevelManager
	{
		std::vector<Level> m_levels;
	};
}