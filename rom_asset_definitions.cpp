#include "rom/rom_asset_definitions.h"
namespace spintool::rom
{
	LevelDataOffsets::LevelDataOffsets(const int level_index)
	{
		const auto table_offsets = LevelDataTableOffsets{};

		foreground_tileset = table_offsets.foreground_tileset + (sizeof(Ptr32) * level_index);
		background_tileset = table_offsets.background_tileset + (sizeof(Ptr32) * level_index);
		foreground_tile_layout = table_offsets.foreground_tile_layout + (sizeof(Ptr32) * level_index);
		background_tile_layout = table_offsets.background_tile_layout + (sizeof(Ptr32) * level_index);
		foreground_tile_brushes = table_offsets.foreground_tile_brushes + (sizeof(Ptr32) * level_index);
		background_tile_brushes = table_offsets.background_tile_brushes + (sizeof(Ptr32) * level_index);
		collision_tiles = table_offsets.collision_tiles + (sizeof(Ptr32) * level_index);
		palette_set = table_offsets.palette_set + (sizeof(Ptr32) * level_index);
		tile_layout_width = table_offsets.tile_layout_width + (sizeof(Ptr32) * level_index);
		tile_layout_height = table_offsets.tile_layout_height + (sizeof(Ptr32) * level_index);
		camera_start_position_x = table_offsets.camera_start_position_x + (sizeof(Ptr32) * level_index);
		camera_start_position_y = table_offsets.camera_start_position_y + (sizeof(Ptr32) * level_index);
		music_id = table_offsets.music_id + (sizeof(Ptr32) * level_index);
		game_over_timer = table_offsets.game_over_timer + (sizeof(Ptr32) * level_index);
		switch_hit_msg_id = table_offsets.switch_hit_msg_id + (sizeof(Ptr32) * level_index);
		bumper_hit_msg_id = table_offsets.bumper_hit_msg_id + (sizeof(Ptr32) * level_index);
		collision_type0_animation_id = table_offsets.collision_type0_animation_id + (sizeof(Ptr32) * level_index);
		collision_type1_animation_id = table_offsets.collision_type1_animation_id + (sizeof(Ptr32) * level_index);
		collision_type2_animation_id = table_offsets.collision_type2_animation_id + (sizeof(Ptr32) * level_index);
		collision_type3_animation_id = table_offsets.collision_type3_animation_id + (sizeof(Ptr32) * level_index);
		collision_bumper_animation_id = table_offsets.collision_bumper_animation_id + (sizeof(Ptr32) * level_index);
		teleporter_animation_id = table_offsets.teleporter_animation_id + (sizeof(Ptr32) * level_index);
		teleporter_y_offset = table_offsets.teleporter_y_offset + (sizeof(Ptr32) * level_index);
		player_start_position_x = table_offsets.player_start_position_x + (sizeof(Ptr32) * level_index);
		player_start_position_y = table_offsets.player_start_position_y + (sizeof(Ptr32) * level_index);
		emerald_count = table_offsets.emerald_count + (sizeof(Ptr32) * level_index);
		flipper_data = table_offsets.flipper_data + (sizeof(Ptr32) * level_index);
		flipper_object_definition = table_offsets.flipper_object_definition + (sizeof(Ptr32) * level_index);
		flipper_animations = table_offsets.flipper_animations + (sizeof(Ptr32) * level_index);
		flipper_count = table_offsets.flipper_count + (sizeof(Ptr32) * level_index);
		flipper_collision_unknown = table_offsets.flipper_collision_unknown + (sizeof(Ptr32) * level_index);
		level_name = table_offsets.level_name + (sizeof(Ptr32) * level_index);
		animations = table_offsets.animations + (sizeof(Ptr32) * level_index);
		ring_count = table_offsets.ring_count + (sizeof(Ptr32) * level_index);
		rom_level_data_BG_tile_vdp_offset = table_offsets.rom_level_data_BG_tile_vdp_offset + (sizeof(Ptr32) * level_index);
	}
}
