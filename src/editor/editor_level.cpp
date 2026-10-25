#include "editor/editor_level.h"

#include "nlohmann/json.hpp"
#include "rom/level.h"
#include "rom/animated_object.h"
#include "rom/spinball_rom.h"
#include "rom/culling_tables/spline_culling_table.h"
#include "rom/culling_tables/game_obj_collision_culling_table.h"
#include "rom/culling_tables/animated_object_culling_table.h"

#include <algorithm>
#include <iostream>

namespace
{
	constexpr int level_count = 4;

	bool RangeIsValid(const spintool::rom::SpinballROM& rom, const Uint32 offset, const std::size_t length)
	{
		const std::size_t rom_size = rom.m_buffer.size();
		const std::size_t start = static_cast<std::size_t>(offset);
		return start <= rom_size && length <= (rom_size - start);
	}

	template<typename T>
	nlohmann::json SerialiseArray(const std::vector<T>& entries)
	{
		nlohmann::json array_json = nlohmann::json::array();
		for (const T& entry : entries)
		{
			array_json.emplace_back(entry.SerialiseToJSON());
		}
		return array_json;
	}

	template<typename T>
	std::vector<T> DeserialiseArray(const nlohmann::json& array_context)
	{
		std::vector<T> entries;
		entries.reserve(array_context.size());
		for (const nlohmann::json& entry_context : array_context)
		{
			entries.emplace_back(T::DeserialiseFromJSON(entry_context));
		}
		return entries;
	}
}

namespace spintool::editor
{
	nlohmann::json LevelProperties::SerialiseToJSON() const
	{
		nlohmann::json properties_json;
		properties_json["level_name"] = level_name;
		properties_json["palette_set"] = palette_set;
		properties_json["tile_layout_width"] = tile_layout_width;
		properties_json["tile_layout_height"] = tile_layout_height;
		properties_json["camera_start_position_x"] = camera_start_position_x;
		properties_json["camera_start_position_y"] = camera_start_position_y;
		properties_json["player_start_position_x"] = player_start_position_x;
		properties_json["player_start_position_y"] = player_start_position_y;
		properties_json["music_id"] = music_id;
		properties_json["game_over_timer"] = game_over_timer;
		properties_json["switch_hit_msg_id"] = switch_hit_msg_id;
		properties_json["bumper_hit_msg_id"] = bumper_hit_msg_id;
		properties_json["collision_type0_animation_id"] = collision_type0_animation_id;
		properties_json["collision_type1_animation_id"] = collision_type1_animation_id;
		properties_json["collision_type2_animation_id"] = collision_type2_animation_id;
		properties_json["collision_type3_animation_id"] = collision_type3_animation_id;
		properties_json["collision_bumper_animation_id"] = collision_bumper_animation_id;
		properties_json["teleporter_animation"] = teleporter_animation;
		properties_json["teleporter_y_offset"] = teleporter_y_offset;
		if (flipper_definition)
		{
			properties_json["flipper_definition"] = flipper_definition->SerialiseToJSON();
		}
		properties_json["flipper_animations"] = flipper_animations;
		properties_json["animations"] = animations;
		properties_json["sprite_table"] = sprite_table;
		properties_json["animation_count"] = animation_count;
		properties_json["bg_tile_vdp_offset"] = bg_tile_vdp_offset;

		return properties_json;
	}

	LevelProperties LevelProperties::DeserialiseFromJSON(const nlohmann::json& properties_context)
	{
		LevelProperties properties;
		properties.level_name = properties_context.at("level_name");
		properties.palette_set = properties_context.at("palette_set");
		properties.tile_layout_width = properties_context.at("tile_layout_width");
		properties.tile_layout_height = properties_context.at("tile_layout_height");
		properties.camera_start_position_x = properties_context.at("camera_start_position_x");
		properties.camera_start_position_y = properties_context.at("camera_start_position_y");
		properties.player_start_position_x = properties_context.at("player_start_position_x");
		properties.player_start_position_y = properties_context.at("player_start_position_y");
		properties.music_id = properties_context.at("music_id");
		properties.game_over_timer = properties_context.at("game_over_timer");
		properties.switch_hit_msg_id = properties_context.at("switch_hit_msg_id");
		properties.bumper_hit_msg_id = properties_context.at("bumper_hit_msg_id");
		properties.collision_type0_animation_id = properties_context.at("collision_type0_animation_id");
		properties.collision_type1_animation_id = properties_context.at("collision_type1_animation_id");
		properties.collision_type2_animation_id = properties_context.at("collision_type2_animation_id");
		properties.collision_type3_animation_id = properties_context.at("collision_type3_animation_id");
		properties.collision_bumper_animation_id = properties_context.at("collision_bumper_animation_id");
		properties.teleporter_animation = properties_context.at("teleporter_animation");
		properties.teleporter_y_offset = properties_context.at("teleporter_y_offset");
		if (properties_context.contains("flipper_definition"))
		{
			properties.flipper_definition = AnimObjectDefinition::DeserialiseFromJSON(properties_context.at("flipper_definition"));
		}
		properties.flipper_animations = properties_context.at("flipper_animations");
		properties.animations = properties_context.at("animations");
		properties.sprite_table = properties_context.at("sprite_table");
		properties.animation_count = properties_context.at("animation_count");
		properties.bg_tile_vdp_offset = properties_context.at("bg_tile_vdp_offset");

		return properties;
	}

	nlohmann::json Level::SerialiseToJSON() const
	{
		nlohmann::json level_json;
		level_json["level_index"] = m_level_index;
		level_json["properties"] = m_properties.SerialiseToJSON();
		level_json["tile_layers"] = SerialiseArray(m_tile_layers);
		level_json["rings"] = SerialiseArray(m_ring_instances);
		level_json["flippers"] = SerialiseArray(m_flipper_instances);
		level_json["game_objects"] = SerialiseArray(m_game_obj_instances);
		level_json["anim_obj_definitions"] = SerialiseArray(m_anim_obj_definitions);
		level_json["collision_splines"] = SerialiseArray(m_collision_splines);

		return level_json;
	}

	Level Level::DeserialiseFromJSON(const nlohmann::json& level_context)
	{
		Level level;
		level.m_level_index = level_context.at("level_index");
		level.m_properties = LevelProperties::DeserialiseFromJSON(level_context.at("properties"));
		level.m_tile_layers = DeserialiseArray<TileLayer>(level_context.at("tile_layers"));
		level.m_ring_instances = DeserialiseArray<RingInstance>(level_context.at("rings"));
		level.m_flipper_instances = DeserialiseArray<FlipperInstance>(level_context.at("flippers"));
		level.m_game_obj_instances = DeserialiseArray<GameObjectInstance>(level_context.at("game_objects"));
		level.m_anim_obj_definitions = DeserialiseArray<AnimObjectDefinition>(level_context.at("anim_obj_definitions"));
		level.m_collision_splines = DeserialiseArray<CollisionSpline>(level_context.at("collision_splines"));

		return level;
	}

	Level Level::LoadFromROM(const rom::SpinballROM& src_rom, const int level_index)
	{
		Level new_level;
		new_level.m_level_index = level_index;

		if (level_index < 0 || level_index >= level_count || src_rom.m_buffer.empty())
		{
			std::cerr << "Cannot load editor level " << level_index << ": invalid level index or ROM not loaded\n";
			return new_level;
		}

		// Reuses the ROM loader for the level name, rings and collision splines.
		const rom::Level rom_level = rom::Level::LoadFromROM(src_rom, level_index);
		const rom::LevelDataOffsets& offsets = rom_level.m_data_offsets;

		LevelProperties& properties = new_level.m_properties;
		properties.level_name = rom_level.m_level_name;
		for (size_t i = 0; i < properties.palette_set.size(); ++i)
		{
			properties.palette_set[i] = src_rom.ReadUint16(offsets.palette_set + static_cast<Uint32>(i * sizeof(Uint16)));
		}
		properties.tile_layout_width = src_rom.ReadUint16(offsets.tile_layout_width);
		properties.tile_layout_height = src_rom.ReadUint16(offsets.tile_layout_height);
		properties.camera_start_position_x = src_rom.ReadUint16(offsets.camera_start_position_x);
		properties.camera_start_position_y = src_rom.ReadUint16(offsets.camera_start_position_y);
		properties.player_start_position_x = src_rom.ReadUint16(offsets.player_start_position_x);
		properties.player_start_position_y = src_rom.ReadUint16(offsets.player_start_position_y);
		properties.music_id = src_rom.ReadUint16(offsets.music_id);
		properties.game_over_timer = src_rom.ReadUint16(offsets.game_over_timer);
		properties.switch_hit_msg_id = src_rom.ReadUint16(offsets.switch_hit_msg_id);
		properties.bumper_hit_msg_id = src_rom.ReadUint16(offsets.bumper_hit_msg_id);
		properties.collision_type0_animation_id = src_rom.ReadUint8(offsets.collision_type0_animation_id);
		properties.collision_type1_animation_id = src_rom.ReadUint8(offsets.collision_type1_animation_id);
		properties.collision_type2_animation_id = src_rom.ReadUint8(offsets.collision_type2_animation_id);
		properties.collision_type3_animation_id = src_rom.ReadUint8(offsets.collision_type3_animation_id);
		properties.collision_bumper_animation_id = src_rom.ReadUint8(offsets.collision_bumper_animation_id);
		properties.teleporter_animation = src_rom.ReadUint32(offsets.teleporter_animation);
		properties.teleporter_y_offset = src_rom.ReadUint8(offsets.teleporter_y_offset);
		properties.flipper_animations = src_rom.ReadUint32(offsets.flipper_animations);
		properties.animations = src_rom.ReadUint32(offsets.animations);
		properties.sprite_table = offsets.sprite_table;
		properties.animation_count = static_cast<Uint16>(offsets.animation_count);
		properties.bg_tile_vdp_offset = src_rom.ReadUint16(offsets.rom_level_data_BG_tile_vdp_offset);

		const rom::Ptr32 flipper_definition_offset = src_rom.ReadUint32(offsets.flipper_object_definition);
		if (RangeIsValid(src_rom, flipper_definition_offset, rom::AnimObjectDefinition::s_size_on_rom))
		{
			properties.flipper_definition = AnimObjectDefinition::FromROM(rom::AnimObjectDefinition::LoadFromROM(src_rom, flipper_definition_offset));
		}

		// Background layer first, matching rom::Level::m_tile_layers.
		new_level.m_tile_layers.emplace_back(TileLayer{ src_rom.ReadUint32(offsets.background_tileset), src_rom.ReadUint32(offsets.background_tile_layout), src_rom.ReadUint32(offsets.background_tile_brushes) });
		new_level.m_tile_layers.emplace_back(TileLayer{ src_rom.ReadUint32(offsets.foreground_tileset), src_rom.ReadUint32(offsets.foreground_tile_layout), src_rom.ReadUint32(offsets.foreground_tile_brushes) });

		new_level.m_ring_instances.reserve(rom_level.m_ring_instances.size());
		for (const rom::RingInstance& rom_ring : rom_level.m_ring_instances)
		{
			new_level.m_ring_instances.emplace_back(RingInstance::FromROM(rom_ring));
		}

		// The culling table repeats a spline in every cell it overlaps, so keep each one once.
		if (rom_level.m_spline_culling_table)
		{
			for (const rom::SplineCullingCell& cell : rom_level.m_spline_culling_table->cells)
			{
				for (const rom::CollisionSpline& rom_spline : cell.splines)
				{
					const CollisionSpline spline = CollisionSpline::FromROM(rom_spline);
					const bool found_spline = std::find(std::begin(new_level.m_collision_splines), std::end(new_level.m_collision_splines), spline) != std::end(new_level.m_collision_splines);
					if (found_spline == false)
					{
						new_level.m_collision_splines.emplace_back(spline);
					}
				}
			}
		}

		const rom::Ptr32 flippers_table_begin = src_rom.ReadUint32(offsets.flipper_data);
		const Uint16 num_flippers = src_rom.ReadUint16(offsets.flipper_count);
		if (RangeIsValid(src_rom, flippers_table_begin, num_flippers * rom::FlipperInstance::s_size_on_rom))
		{
			new_level.m_flipper_instances.reserve(num_flippers);
			Uint32 current_table_offset = flippers_table_begin;
			for (Uint16 i = 0; i < num_flippers; ++i)
			{
				const rom::FlipperInstance rom_flipper = rom::FlipperInstance::LoadFromROM(src_rom, current_table_offset);
				new_level.m_flipper_instances.emplace_back(FlipperInstance::FromROM(rom_flipper));
				current_table_offset = rom_flipper.rom_data.rom_offset_end;
			}
		}
		else
		{
			std::cerr << "Skipping invalid flipper table for level " << level_index << '\n';
		}

		if (RangeIsValid(src_rom, offsets.object_instances.offset, offsets.object_instances.count * rom::GameObjectDefinition::s_size_on_rom))
		{
			new_level.m_game_obj_instances.reserve(offsets.object_instances.count);
			Uint32 current_table_offset = offsets.object_instances.offset;
			for (Uint16 i = 0; i < offsets.object_instances.count; ++i)
			{
				const rom::GameObjectDefinition rom_game_obj = rom::GameObjectDefinition::LoadFromROM(src_rom, current_table_offset);
				new_level.m_game_obj_instances.emplace_back(GameObjectInstance::FromROM(rom_game_obj));
				current_table_offset = rom_game_obj.rom_data.rom_offset_end;

				// Animated object definitions are shared between game objects, so keep each one once.
				const rom::Ptr32 anim_def_offset = rom_game_obj.animation_definition;
				const bool found_anim_def = std::any_of(std::begin(new_level.m_anim_obj_definitions), std::end(new_level.m_anim_obj_definitions),
					[anim_def_offset](const AnimObjectDefinition& anim_obj)
					{
						return anim_obj.rom_offset == anim_def_offset;
					});

				if (rom_game_obj.instance_id != 0 && anim_def_offset != 0 && found_anim_def == false
					&& RangeIsValid(src_rom, anim_def_offset, rom::AnimObjectDefinition::s_size_on_rom))
				{
					new_level.m_anim_obj_definitions.emplace_back(AnimObjectDefinition::FromROM(rom::AnimObjectDefinition::LoadFromROM(src_rom, anim_def_offset)));
				}
			}
		}
		else
		{
			std::cerr << "Skipping invalid game object table for level " << level_index << '\n';
		}

		return new_level;
	}
}
