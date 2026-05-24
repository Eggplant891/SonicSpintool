#include "rom/rom_asset_definitions.h"

#include "json.hpp"

namespace spintool::rom
{
	Ptr32 RingAnimationFrameIDs = 0x000D3B2A; // Uint8[4]
	Ptr32 RingAnimationFrameXOffsets = 0x000D3B44; // Uint16[4]

	Ptr32 OSDTileset = 0x0007DA22;

	Ptr32 BonusLevelBGTileLayout = 0x000C6EF0;
	Ptr32 BonusLevelBGTileset = 0x000C77B0;
	Ptr32 BonusLevelTilesetTrappedAlive = 0x000CC710;

	Ptr32 OptionsMenuPaletteSet = 0x0000115C;
	Ptr32 OptionsMenuTileset = 0x000BDD2E;
	Ptr32 OptionsMenuTileBrushes = 0x000BDFBC;
	Ptr32 OptionsMenuTileLayout = 0x000BE1BC;

	Ptr32 IntroCutscenesTileset = 0x000A3124 + 2;
	Ptr32 MainMenuTileset = 0x0009D102 + 2;

	Ptr32 SegaLogoPaletteSet = 0x000993FA;
	Ptr32 SegaLogoTileLayout = 0x000A14F8;

	Ptr32 MainMenuPaletteSet = 0x0009BD3A;
	Ptr32 MainMenuTileLayoutGiantBumper = 0x0009C05A;
	Ptr32 MainMenuTileLayoutBG = 0x0009C82E;

	Ptr32 IntroCutscenePaletteSet = 0x000A1388;
	Ptr32 IntroCutsceneTileLayoutVegOFortress = 0x000A1B8C;
	Ptr32 IntroCutsceneTileLayoutVegOFortressEmptyTopSection = 0x000A2168;
	Ptr32 IntroCutsceneTileLayoutOcean = 0x000A220C;
	Ptr32 IntroCutsceneTileLayoutSky = 0x000A2510;
	Ptr32 IntroCutsceneTileLayoutRobotnikShip = 0x000A30BC;

	void ArrayOffset::Serialise(nlohmann::json &writer)
	{
		writer["offset"] = offset;
		writer["count"] = count;

	}

	void ArrayOffset::Deserialise(const nlohmann::json &reader)
	{
		offset = reader["offset"].get<Ptr32>();
		count = reader["count"].get<Uint16>();
	}

	void LevelDataTableOffsets::Serialise(nlohmann::json &writer)
	{
		writer["foreground_tileset"] = foreground_tileset;
		writer["background_tileset"] = background_tileset;
		writer["foreground_tile_layout"] = foreground_tile_layout;
		writer["background_tile_layout"] = background_tile_layout;
		writer["foreground_tile_brushes"] = foreground_tile_brushes;
		writer["background_tile_brushes"] = background_tile_brushes;
		writer["collision_data_terrain"] = collision_data_terrain;
		writer["palette_set"] = palette_set;
		writer["tile_layout_width"] = tile_layout_width;
		writer["tile_layout_height"] = tile_layout_height;
		writer["camera_start_position_x"] = camera_start_position_x;
		writer["camera_start_position_y"] = camera_start_position_y;
		writer["music_id"] = music_id;
		writer["game_over_timer "] = game_over_timer;
		writer["camera_activation_sector_anim_obj_ids"] = camera_activation_sector_anim_obj_ids;
		writer["switch_hit_msg_id "] = switch_hit_msg_id;
		writer["bumper_hit_msg_id "] = bumper_hit_msg_id;
		writer["collision_type0_animation_id"] = collision_type0_animation_id;
		writer["collision_type1_animation_id"] = collision_type1_animation_id;
		writer["collision_type2_animation_id"] = collision_type2_animation_id;
		writer["collision_type3_animation_id"] = collision_type3_animation_id;
		writer["collision_bumper_animation_id"] = collision_bumper_animation_id;
		writer["teleporter_animation_id"] = teleporter_animation_id;
		writer["teleporter_y_offset"] = teleporter_y_offset;
		writer["player_start_position_x"] = player_start_position_x;
		writer["player_start_position_y"] = player_start_position_y;
		writer["emerald_count"] = emerald_count;
		writer["flipper_data"] = flipper_data;
		writer["flipper_object_definition"] = flipper_object_definition;
		writer["flipper_animations"] = flipper_animations;
		writer["flipper_count"] = flipper_count;
		writer["flipper_collision_unknown"] = flipper_collision_unknown;
		writer["level_name"] = level_name;
		writer["animations"] = animations;
		writer["ring_count"] = ring_count;
		writer["rom_level_data_BG_tile_vdp_offset"] = rom_level_data_BG_tile_vdp_offset;

		writer["sprite_tables"] = sprite_table;
		writer["animation_count"] = animation_count;

		{
			auto array_writer = writer["collision_tile_obj_ids"].array();
			for (auto& array_count_entry : collision_tile_obj_ids)
			{
				auto& array_entry_writer = array_writer.emplace_back();
				array_count_entry.Serialise(array_entry_writer);
			}
			writer["collision_tile_obj_ids"] = array_writer;
		}

		{
			auto array_writer = writer["ring_instances"].array();
			for (auto& array_count_entry : ring_instances)
			{
				auto& array_entry_writer = array_writer.emplace_back();
				array_count_entry.Serialise(array_entry_writer);
			}
			writer["ring_instances"] = array_writer;
		}

		{
			auto array_writer = writer["object_instances"].array();
			for (auto& array_count_entry : object_instances)
			{
				auto& array_entry_writer = array_writer.emplace_back();
				array_count_entry.Serialise(array_entry_writer);
			}
			writer["object_instances"] = array_writer;
		}
	}

	void LevelDataTableOffsets::Deserialise(const nlohmann::json &reader)
	{
		reader;
	}

	LevelDataOffsets::LevelDataOffsets(const int level_index)
		: table_offsets(LevelDataTableOffsets{})
		, foreground_tileset(table_offsets.foreground_tileset + (sizeof(Ptr32) * level_index))
		, background_tileset(table_offsets.background_tileset + (sizeof(Ptr32) * level_index))
		, foreground_tile_layout(table_offsets.foreground_tile_layout + (sizeof(Ptr32) * level_index))
		, background_tile_layout(table_offsets.background_tile_layout + (sizeof(Ptr32) * level_index))
		, foreground_tile_brushes(table_offsets.foreground_tile_brushes + (sizeof(Ptr32) * level_index))
		, background_tile_brushes(table_offsets.background_tile_brushes + (sizeof(Ptr32) * level_index))
		, collision_data_terrain(table_offsets.collision_data_terrain + (sizeof(Ptr32) * level_index))
		, palette_set(table_offsets.palette_set + (sizeof(Uint16[4]) * level_index))
		, tile_layout_width(table_offsets.tile_layout_width + (sizeof(Uint16[2]) * level_index))
		, tile_layout_height(table_offsets.tile_layout_height + (sizeof(Uint16[2]) * level_index))
		, camera_start_position_x(table_offsets.camera_start_position_x + (sizeof(Uint16[2]) * level_index))
		, camera_start_position_y(table_offsets.camera_start_position_y + (sizeof(Uint16[2]) * level_index))
		, music_id(table_offsets.music_id + (sizeof(Uint16) * level_index))
		, game_over_timer(table_offsets.game_over_timer + (sizeof(Uint16) * level_index))
		, camera_activation_sector_anim_obj_ids(table_offsets.camera_activation_sector_anim_obj_ids + (sizeof(Ptr32) * level_index))
		, switch_hit_msg_id(table_offsets.switch_hit_msg_id + (sizeof(Uint16) * level_index))
		, bumper_hit_msg_id(table_offsets.bumper_hit_msg_id + (sizeof(Uint16) * level_index))
		, collision_type0_animation_id(table_offsets.collision_type0_animation_id + (sizeof(Uint8) * level_index))
		, collision_type1_animation_id(table_offsets.collision_type1_animation_id + (sizeof(Uint8) * level_index))
		, collision_type2_animation_id(table_offsets.collision_type2_animation_id + (sizeof(Uint8) * level_index))
		, collision_type3_animation_id(table_offsets.collision_type3_animation_id + (sizeof(Uint8) * level_index))
		, collision_bumper_animation_id(table_offsets.collision_bumper_animation_id + (sizeof(Uint8) * level_index))
		, teleporter_animation(table_offsets.teleporter_animation_id + (sizeof(Ptr32) * level_index))
		, teleporter_y_offset(table_offsets.teleporter_y_offset + (sizeof(Uint8) * level_index))
		, player_start_position_x(table_offsets.player_start_position_x + (sizeof(Uint16[2]) * level_index))
		, player_start_position_y(table_offsets.player_start_position_y + (sizeof(Uint16[2]) * level_index))
		, emerald_count(table_offsets.emerald_count + (sizeof(Uint8) * level_index))
		, flipper_data(table_offsets.flipper_data + (sizeof(Ptr32) * level_index))
		, flipper_object_definition(table_offsets.flipper_object_definition + (sizeof(Ptr32) * level_index))
		, flipper_animations(table_offsets.flipper_animations + (sizeof(Ptr32) * level_index))
		, flipper_count(table_offsets.flipper_count + (sizeof(Uint16) * level_index))
		, flipper_collision_unknown(table_offsets.flipper_collision_unknown + (sizeof(Ptr32) * level_index))
		, level_name(table_offsets.level_name + (sizeof(Ptr32) * level_index))
		, animations(table_offsets.animations + (sizeof(Ptr32) * level_index))
		, sprite_table(table_offsets.sprite_table[level_index])
		, animation_count(table_offsets.animation_count[level_index])
		, ring_count(table_offsets.ring_count + (sizeof(Ptr32) * level_index))
		, collision_tile_obj_ids(table_offsets.collision_tile_obj_ids[level_index])
		, ring_instances(table_offsets.ring_instances[level_index])
		, object_instances(table_offsets.object_instances[level_index])
		, rom_level_data_BG_tile_vdp_offset(table_offsets.rom_level_data_BG_tile_vdp_offset + (sizeof(Ptr32) * level_index))
	{

	}

	void LevelDataOffsets::Serialise(nlohmann::json &writer)
	{
		writer["foreground_tileset"] = foreground_tileset;
		writer["background_tileset"] = background_tileset;
		writer["foreground_tile_layout"] = foreground_tile_layout;
		writer["background_tile_layout"] = background_tile_layout;
		writer["foreground_tile_brushes"] = foreground_tile_brushes;
		writer["background_tile_brushes"] = background_tile_brushes;
		writer["collision_data_terrain"] = collision_data_terrain;
		writer["palette_set"] = palette_set;
		writer["tile_layout_width"] = tile_layout_width;
		writer["tile_layout_height"] = tile_layout_height;
		writer["camera_start_position_x"] = camera_start_position_x;
		writer["camera_start_position_y"] = camera_start_position_y;
		writer["music_id"] = music_id;
		writer["game_over_timer"] = game_over_timer;
		writer["camera_activation_sector_anim_obj_ids"] = camera_activation_sector_anim_obj_ids;
		writer["switch_hit_msg_id"] = switch_hit_msg_id;
		writer["bumper_hit_msg_id"] = bumper_hit_msg_id;
		writer["collision_type0_animation_id"] = collision_type0_animation_id;
		writer["collision_type1_animation_id"] = collision_type1_animation_id;
		writer["collision_type2_animation_id"] = collision_type2_animation_id;
		writer["collision_type3_animation_id"] = collision_type3_animation_id;
		writer["collision_bumper_animation_id"] = collision_bumper_animation_id;
		writer["teleporter_animation"] = teleporter_animation;
		writer["teleporter_y_offset"] = teleporter_y_offset;
		writer["player_start_position_x"] = player_start_position_x;
		writer["player_start_position_y"] = player_start_position_y;
		writer["emerald_count"] = emerald_count;
		writer["flipper_data"] = flipper_data;
		writer["flipper_object_definition"] = flipper_object_definition;
		writer["flipper_animations"] = flipper_animations;
		writer["flipper_count"] = flipper_count;
		writer["flipper_collision_unknown"] = flipper_collision_unknown;
		writer["level_name"] = level_name;
		writer["animations"] = animations;
		writer["sprite_table"] = sprite_table;
		writer["animation_count"] = animation_count;
		writer["ring_count"] = ring_count;
		collision_tile_obj_ids.Serialise(writer["collision_tile_obj_ids"]);
		ring_instances.Serialise(writer["ring_instances"]);
		object_instances.Serialise(writer["object_instances"]);
		writer["rom_level_data_BG_tile_vdp_offset"] = rom_level_data_BG_tile_vdp_offset;
	}

	void LevelDataOffsets::Deserialise(const nlohmann::json &reader)
	{

	}
}
