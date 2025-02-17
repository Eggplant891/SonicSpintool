#include "rom/rom_asset_definitions.h"

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

	LevelDataOffsets::LevelDataOffsets(const int level_index)
		: table_offsets(LevelDataTableOffsets{})
		, foreground_tileset(table_offsets.foreground_tileset + (sizeof(Ptr32) * level_index))
		, background_tileset(table_offsets.background_tileset + (sizeof(Ptr32) * level_index))
		, foreground_tile_layout(table_offsets.foreground_tile_layout + (sizeof(Ptr32) * level_index))
		, background_tile_layout(table_offsets.background_tile_layout + (sizeof(Ptr32) * level_index))
		, foreground_tile_brushes(table_offsets.foreground_tile_brushes + (sizeof(Ptr32) * level_index))
		, background_tile_brushes(table_offsets.background_tile_brushes + (sizeof(Ptr32) * level_index))
		, collision_tiles(table_offsets.collision_tiles + (sizeof(Ptr32) * level_index))
		, palette_set(table_offsets.palette_set + (sizeof(Uint16[4]) * level_index))
		, tile_layout_width(table_offsets.tile_layout_width + (sizeof(Uint16[2]) * level_index))
		, tile_layout_height(table_offsets.tile_layout_height + (sizeof(Uint16[2]) * level_index))
		, camera_start_position_x(table_offsets.camera_start_position_x + (sizeof(Uint16[2]) * level_index))
		, camera_start_position_y(table_offsets.camera_start_position_y + (sizeof(Uint16[2]) * level_index))
		, music_id(table_offsets.music_id + (sizeof(Uint16) * level_index))
		, game_over_timer(table_offsets.game_over_timer + (sizeof(Uint16) * level_index))
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
		, ring_count(table_offsets.ring_count + (sizeof(Ptr32) * level_index))
		, ring_instances(table_offsets.ring_instances[level_index])
		, object_instances(table_offsets.object_instances[level_index])
		, rom_level_data_BG_tile_vdp_offset(table_offsets.rom_level_data_BG_tile_vdp_offset + (sizeof(Ptr32) * level_index))
	{

	}
}
