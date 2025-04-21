#pragma once

#include "types/rom_ptr.h"
#include <utility>

namespace spintool::rom
{
	const Ptr32 PaletteRows = 0x00000DFC;

	struct ArrayOffset
	{
		Uint32 offset = 0;
		Uint16 count = 0;
	};

	struct LevelDataTableOffsets
	{
		Ptr32 foreground_tileset = 0x000BFBE0;
		Ptr32 background_tileset = 0x000BFBF0;
		Ptr32 foreground_tile_layout = 0x000BFC00;
		Ptr32 background_tile_layout = 0x000BFC10;
		Ptr32 foreground_tile_brushes = 0x000BFC20;
		Ptr32 background_tile_brushes = 0x000BFC30;
		Ptr32 collision_data_terrain = 0x000BFC40;
		Ptr32 palette_set = 0x000BFC50;
		Ptr32 tile_layout_width = 0x000BFC70;
		Ptr32 tile_layout_height = 0x000BFC72;
		Ptr32 camera_start_position_x = 0x000BFC80;
		Ptr32 camera_start_position_y = 0x000BFC80;
		Ptr32 music_id = 0x000BFC90;
		Ptr32 game_over_timer = 0x000BFC98 ;
		Ptr32 camera_activation_sector_anim_obj_ids = 0x000BFD14;
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

		Ptr32 sprite_table[4] = {
			0x12B0C,
			0x00020666,
			0x0002B7A4,
			0x000318EE
		};

		Uint16 animation_count[4] = {
			63,
			36,
			40,
			57
		};

		ArrayOffset collision_tile_obj_ids[4] = {
			{ 0x000C326E, 0x100 }, // Toxic Caves
			{ 0x000C1764, 0x100 }, // Lava Powerhouse
			{ 0x000C5B9C, 0x100 }, // The Machine
			{ 0x0, 0x0 }		   // Showdown
		};

		ArrayOffset ring_instances[4] = {
			{ 0x000C3854, 0x3A }, // Toxic Caves
			{ 0x000C1D70, 0x43 }, // Lava Powerhouse
			{ 0x000C60B2, 0x51 }, // The Machine
			{ 0x000C49CC, 0x3E }  // Showdown
		};

		ArrayOffset object_instances[4] = {
			{ 0x000C39B0, 0xAD }, // Toxic Caves
			{ 0x000C1F02, 0xD3 }, // Lava Powerhouse
			{ 0x000C6298, 0x9E }, // The Machine
			{ 0x000C4B40, 0xBA }  // Showdown
		};
	};

	struct LevelDataOffsets
	{
		LevelDataOffsets() = default;
		LevelDataOffsets(const int level_index);

		LevelDataTableOffsets table_offsets;
		Ptr32 foreground_tileset = 0;
		Ptr32 background_tileset = 0;
		Ptr32 foreground_tile_layout = 0;
		Ptr32 background_tile_layout = 0;
		Ptr32 foreground_tile_brushes = 0;
		Ptr32 background_tile_brushes = 0;
		Ptr32 collision_data_terrain = 0;
		Ptr32 palette_set = 0;
		Ptr32 tile_layout_width = 0;
		Ptr32 tile_layout_height = 0;
		Ptr32 camera_start_position_x = 0;
		Ptr32 camera_start_position_y = 0;
		Ptr32 music_id = 0;
		Ptr32 game_over_timer = 0;
		Ptr32 camera_activation_sector_anim_obj_ids = 0;
		Ptr32 switch_hit_msg_id = 0;
		Ptr32 bumper_hit_msg_id = 0;
		Ptr32 collision_type0_animation_id = 0;
		Ptr32 collision_type1_animation_id = 0;
		Ptr32 collision_type2_animation_id = 0;
		Ptr32 collision_type3_animation_id = 0;
		Ptr32 collision_bumper_animation_id = 0;
		Ptr32 teleporter_animation = 0;
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
		Ptr32 sprite_table = 0;
		Ptr32 animation_count = 0;
		Ptr32 ring_count = 0;
		ArrayOffset collision_tile_obj_ids;
		ArrayOffset ring_instances;
		ArrayOffset object_instances;
		Ptr32 rom_level_data_BG_tile_vdp_offset = 0;
	};

	////////////////////////////////////////////////////////////////////////////////////////

	extern Ptr32 RingAnimationFrameIDs;
	extern Ptr32 RingAnimationFrameXOffsets;
	extern Ptr32 OSDTileset;
	extern Ptr32 BonusLevelBGTileLayout;
	extern Ptr32 BonusLevelBGTileset;
	extern Ptr32 BonusLevelTilesetTrappedAlive;
	extern Ptr32 OptionsMenuPaletteSet;
	extern Ptr32 OptionsMenuTileset;
	extern Ptr32 OptionsMenuTileBrushes;
	extern Ptr32 OptionsMenuTileLayout;
	extern Ptr32 IntroCutscenesTileset;
	extern Ptr32 MainMenuTileset;
	extern Ptr32 SegaLogoPaletteSet;
	extern Ptr32 SegaLogoTileLayout;
	extern Ptr32 MainMenuPaletteSet;
	extern Ptr32 MainMenuTileLayoutGiantBumper;
	extern Ptr32 MainMenuTileLayoutBG;
	extern Ptr32 IntroCutscenePaletteSet;
	extern Ptr32 IntroCutsceneTileLayoutVegOFortress;
	extern Ptr32 IntroCutsceneTileLayoutVegOFortressEmptyTopSection;
	extern Ptr32 IntroCutsceneTileLayoutOcean;
	extern Ptr32 IntroCutsceneTileLayoutSky;
	extern Ptr32 IntroCutsceneTileLayoutRobotnikShip;

}