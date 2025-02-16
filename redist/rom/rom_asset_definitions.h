#pragma once

#include "types/rom_ptr.h"

namespace spintool::rom
{
	const Ptr32 PaletteRows = 0x00000DFC;

	struct LevelDataTableOffsets
	{

		Ptr32 foreground_tileset = 0x000BFBE0;
		Ptr32 background_tileset = 0x000BFBF0;
		Ptr32 foreground_tile_layout = 0x000BFC00;
		Ptr32 background_tile_layout = 0x000BFC10;
		Ptr32 foreground_tile_brushes = 0x000BFC20;
		Ptr32 background_tile_brushes = 0x000BFC30;
		Ptr32 collision_tiles = 0x000BFC40;
		Ptr32 palette_set = 0x000BFC50;
		Ptr32 tile_layout_width = 0x000BFC70;
		Ptr32 tile_layout_height = 0x000BFC72;
		Ptr32 camera_start_position_x = 0x000BFC80;
		Ptr32 camera_start_position_y = 0x000BFC80;
		Ptr32 music_id = 0x000BFC90;
		Ptr32 game_over_timer = 0x000BFC98 ;
		Ptr32 switch_hit_msg_id = 0x000C06BA ;
		Ptr32 bumper_hit_msg_id = 0x000C06C2 ;
		Ptr32 collision_type0_animation_id = 0x000C06CE;
		Ptr32 collision_type1_animation_id = 0x000C06D2;
		Ptr32 collision_type2_animation_id = 0x000C06D6;
		Ptr32 collision_type3_animation_id = 0x000C06CA;
		Ptr32 collision_bumper_animation_id = 0x000C06DA;
		Ptr32 teleporter_animation_id = 0x000C06DE;
		Ptr32 teleporter_y_offset = 0x000C06EE;
		Ptr32 player_start_position_x = 0x000C06F2;
		Ptr32 player_start_position_y = 0x000C06F4;
		Ptr32 emerald_count = 0x000C0702;
		Ptr32 flipper_data = 0x000C08BE;
		Ptr32 flipper_object_definition = 0x000C08CE;
		Ptr32 flipper_animations = 0x000C08DE;
		Ptr32 flipper_count = 0x000C08EE ;
		Ptr32 flipper_collision_unknown = 0x000C0A96;
		Ptr32 level_name = 0x000C14FC;
		Ptr32 animations = 0x000C150C;
		Ptr32 ring_count = 0x000C151C;
		Ptr32 rom_level_data_BG_tile_vdp_offset = 0x000D8D04;
	};

	struct LevelDataOffsets
	{
		LevelDataOffsets(const int level_index);

		Ptr32 foreground_tileset = 0;
		Ptr32 background_tileset = 0;
		Ptr32 foreground_tile_layout = 0;
		Ptr32 background_tile_layout = 0;
		Ptr32 foreground_tile_brushes = 0;
		Ptr32 background_tile_brushes = 0;
		Ptr32 collision_tiles = 0;
		Ptr32 palette_set = 0;
		Ptr32 tile_layout_width = 0;
		Ptr32 tile_layout_height = 0;
		Ptr32 camera_start_position_x = 0;
		Ptr32 camera_start_position_y = 0;
		Ptr32 music_id = 0;
		Ptr32 game_over_timer = 0;
		Ptr32 switch_hit_msg_id = 0;
		Ptr32 bumper_hit_msg_id = 0;
		Ptr32 collision_type0_animation_id = 0;
		Ptr32 collision_type1_animation_id = 0;
		Ptr32 collision_type2_animation_id = 0;
		Ptr32 collision_type3_animation_id = 0;
		Ptr32 collision_bumper_animation_id = 0;
		Ptr32 teleporter_animation_id = 0;
		Ptr32 teleporter_y_offset = 0;
		Ptr32 player_start_position_x = 0;
		Ptr32 player_start_position_y = 0;
		Ptr32 emerald_count = 0;
		Ptr32 flipper_data = 0;
		Ptr32 flipper_object_definition = 0;
		Ptr32 flipper_animations = 0;
		Ptr32 flipper_count = 0;
		Ptr32 flipper_collision_unknown = 0;
		Ptr32 level_name = 0;
		Ptr32 animations = 0;
		Ptr32 ring_count = 0;
		Ptr32 rom_level_data_BG_tile_vdp_offset = 0;
	};

	////////////////////////////////////////////////////////////////////////////////////////

	Ptr32 RingAnimationFrameIDs = 0x000D3B2A; // Uint8[4]
	Ptr32 RingAnimationFrameXOffsets = 0x000D3B44; // Uint16[4]

	Ptr32 OSDTileset = 0x0007DA22;

	Ptr32 BonusLevelBGTileLayout = 0x000C6EF0;
	Ptr32 BonusLevelBGTileset = 0x000C77B0;
	Ptr32 BonusLevelTilesetTrappedAlive = 0x000CC710;

	Ptr32 OptionsMenuPaletteSet = 0x0000115C;
	Ptr32 OptionsMenuTileset = 0x000BDD2E;
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

}