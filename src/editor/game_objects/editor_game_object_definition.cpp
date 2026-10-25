#include "editor/game_objects/editor_game_object_definition.h"

#include "nlohmann/json.hpp"
#include "rom/game_objects/game_object_definition.h"
#include "rom/culling_tables/spline_culling_table.h"

namespace spintool::editor
{
	nlohmann::json GameObjectInstance::SerialiseToJSON() const
	{
		nlohmann::json game_obj_json;
		game_obj_json["type_id"] = type_id;
		game_obj_json["instance_id"] = instance_id;
		game_obj_json["unk_1"] = unk_1;
		game_obj_json["unk_2"] = unk_2;
		game_obj_json["x_pos"] = x_pos;
		game_obj_json["y_pos"] = y_pos;
		game_obj_json["collision_width"] = collision_width;
		game_obj_json["collision_height"] = collision_height;
		game_obj_json["animation_definition"] = animation_definition;
		if (collision)
		{
			game_obj_json["collision"] = collision->SerialiseToJSON();
		}
		game_obj_json["flags"] = flags;
		game_obj_json["flip_x"] = flip_x;
		game_obj_json["flip_y"] = flip_y;

		return game_obj_json;
	}

	GameObjectInstance GameObjectInstance::DeserialiseFromJSON(const nlohmann::json& game_obj_context)
	{
		GameObjectInstance game_obj;
		game_obj.type_id = game_obj_context.at("type_id");
		game_obj.instance_id = game_obj_context.at("instance_id");
		game_obj.unk_1 = game_obj_context.at("unk_1");
		game_obj.unk_2 = game_obj_context.at("unk_2");
		game_obj.x_pos = game_obj_context.at("x_pos");
		game_obj.y_pos = game_obj_context.at("y_pos");
		game_obj.collision_width = game_obj_context.at("collision_width");
		game_obj.collision_height = game_obj_context.at("collision_height");
		game_obj.animation_definition = game_obj_context.at("animation_definition");
		if (game_obj_context.contains("collision"))
		{
			game_obj.collision = CollisionSpline::DeserialiseFromJSON(game_obj_context.at("collision"));
		}
		game_obj.flags = game_obj_context.at("flags");
		game_obj.flip_x = game_obj_context.at("flip_x");
		game_obj.flip_y = game_obj_context.at("flip_y");

		return game_obj;
	}

	GameObjectInstance GameObjectInstance::FromROM(const rom::GameObjectDefinition& rom_game_obj)
	{
		GameObjectInstance game_obj;
		game_obj.type_id = static_cast<Uint8>(rom_game_obj.type_id);
		game_obj.instance_id = rom_game_obj.instance_id;
		game_obj.unk_1 = rom_game_obj.unk_1;
		game_obj.unk_2 = rom_game_obj.unk_2;
		game_obj.x_pos = rom_game_obj.x_pos;
		game_obj.y_pos = rom_game_obj.y_pos;
		game_obj.collision_width = rom_game_obj.collision_width;
		game_obj.collision_height = rom_game_obj.collision_height;
		game_obj.animation_definition = rom_game_obj.animation_definition;
		if (rom_game_obj.collision)
		{
			game_obj.collision = CollisionSpline::FromROM(*rom_game_obj.collision);
		}
		game_obj.flags = rom_game_obj.flags;
		game_obj.flip_x = rom_game_obj.flip_x;
		game_obj.flip_y = rom_game_obj.flip_y;

		return game_obj;
	}
}
