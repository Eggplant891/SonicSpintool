#include "editor/editor_animated_object.h"

#include "nlohmann/json.hpp"
#include "rom/animated_object.h"

namespace spintool::editor
{
	nlohmann::json AnimObjectDefinition::SerialiseToJSON() const
	{
		nlohmann::json anim_obj_json;
		anim_obj_json["rom_offset"] = rom_offset;
		anim_obj_json["sprite_table"] = sprite_table;
		anim_obj_json["starting_animation"] = starting_animation;
		anim_obj_json["palette_index"] = palette_index;
		anim_obj_json["frame_time_ticks"] = frame_time_ticks;
		anim_obj_json["unknown_1"] = unknown_1;
		anim_obj_json["unknown_2"] = unknown_2;

		return anim_obj_json;
	}

	AnimObjectDefinition AnimObjectDefinition::DeserialiseFromJSON(const nlohmann::json& anim_obj_context)
	{
		AnimObjectDefinition anim_obj;
		anim_obj.rom_offset = anim_obj_context.at("rom_offset");
		anim_obj.sprite_table = anim_obj_context.at("sprite_table");
		anim_obj.starting_animation = anim_obj_context.at("starting_animation");
		anim_obj.palette_index = anim_obj_context.at("palette_index");
		anim_obj.frame_time_ticks = anim_obj_context.at("frame_time_ticks");
		anim_obj.unknown_1 = anim_obj_context.at("unknown_1");
		anim_obj.unknown_2 = anim_obj_context.at("unknown_2");

		return anim_obj;
	}

	AnimObjectDefinition AnimObjectDefinition::FromROM(const rom::AnimObjectDefinition& rom_anim_obj)
	{
		AnimObjectDefinition anim_obj;
		anim_obj.rom_offset = rom_anim_obj.rom_data.rom_offset;
		anim_obj.sprite_table = rom_anim_obj.sprite_table;
		anim_obj.starting_animation = rom_anim_obj.starting_animation;
		anim_obj.palette_index = rom_anim_obj.palette_index;
		anim_obj.frame_time_ticks = rom_anim_obj.frame_time_ticks;
		anim_obj.unknown_1 = rom_anim_obj.unknown_1;
		anim_obj.unknown_2 = rom_anim_obj.unknown_2;

		return anim_obj;
	}
}
