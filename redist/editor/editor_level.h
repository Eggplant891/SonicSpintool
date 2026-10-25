#pragma once

#include "editor/editor_animated_object.h"
#include "editor/editor_collision_spline.h"
#include "editor/editor_tile_layer.h"
#include "editor/game_objects/editor_game_object_definition.h"
#include "editor/game_objects/editor_game_object_flipper.h"
#include "editor/game_objects/editor_game_object_ring.h"
#include "types/rom_ptr.h"
#include "rom/metadata/level_metadata.h"
#include "nlohmann/json_fwd.hpp"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace spintool::rom
{
	class SpinballROM;
}

namespace spintool::editor
{
	// Values stored at the per-level locations described by rom::LevelDataOffsets.
	// Counts (rings, flippers, emeralds) are derived from the instance lists rather than stored.
	struct LevelProperties
	{
		std::string level_name;
		std::array<Uint16, rom::s_max_palettes> palette_set{};
		Uint16 tile_layout_width = 0;
		Uint16 tile_layout_height = 0;
		Uint16 camera_start_position_x = 0;
		Uint16 camera_start_position_y = 0;
		Uint16 player_start_position_x = 0;
		Uint16 player_start_position_y = 0;
		Uint16 music_id = 0;
		Uint16 game_over_timer = 0;
		Uint16 switch_hit_msg_id = 0;
		Uint16 bumper_hit_msg_id = 0;
		Uint8 collision_type0_animation_id = 0;
		Uint8 collision_type1_animation_id = 0;
		Uint8 collision_type2_animation_id = 0;
		Uint8 collision_type3_animation_id = 0;
		Uint8 collision_bumper_animation_id = 0;
		rom::Ptr32 teleporter_animation = 0;
		Uint8 teleporter_y_offset = 0;
		std::optional<AnimObjectDefinition> flipper_definition;
		rom::Ptr32 flipper_animations = 0;
		rom::Ptr32 animations = 0;
		rom::Ptr32 sprite_table = 0;
		Uint16 animation_count = 0;
		Uint16 bg_tile_vdp_offset = 0;

		[[nodiscard]] nlohmann::json SerialiseToJSON() const;
		[[nodiscard]] static LevelProperties DeserialiseFromJSON(const nlohmann::json& properties_context);
	};

	class Level
	{
	public:
		std::string level_filename;
		int m_level_index = -1;
		LevelProperties m_properties;
		std::vector<editor::TileLayer> m_tile_layers;
		std::vector<editor::FlipperInstance> m_flipper_instances;
		std::vector<editor::RingInstance> m_ring_instances;
		std::vector<metadata::LevelMetadata> m_level_metadata;
		std::vector<editor::GameObjectInstance> m_game_obj_instances;
		std::vector<editor::AnimObjectDefinition> m_anim_obj_definitions;
		std::vector<editor::CollisionSpline> m_collision_splines;

		[[nodiscard]] nlohmann::json SerialiseToJSON() const;
		[[nodiscard]] static Level DeserialiseFromJSON(const nlohmann::json& level_context);
		[[nodiscard]] static Level LoadFromROM(const rom::SpinballROM& src_rom, int level_index);
	};
}
