#include "editor/game_objects/editor_game_object_flipper.h"

#include "nlohmann/json.hpp"
#include "rom/game_objects/game_object_flipper.h"

namespace spintool::editor
{
	nlohmann::json FlipperInstance::SerialiseToJSON() const
	{
		nlohmann::json flipper_json;
		flipper_json["x_pos"] = x_pos;
		flipper_json["y_pos"] = y_pos;
		flipper_json["flip_x"] = is_x_flipped;

		return flipper_json;
	}

	FlipperInstance FlipperInstance::DeserialiseFromJSON(const nlohmann::json& flipper_context)
	{
		FlipperInstance flipper;
		flipper.x_pos = flipper_context.at("x_pos");
		flipper.y_pos = flipper_context.at("y_pos");
		flipper.is_x_flipped = flipper_context.at("flip_x");

		return flipper;
	}

	FlipperInstance FlipperInstance::FromROM(const rom::FlipperInstance& rom_flipper)
	{
		FlipperInstance flipper;
		flipper.x_pos = rom_flipper.x_pos;
		flipper.y_pos = rom_flipper.y_pos;
		flipper.is_x_flipped = rom_flipper.is_x_flipped;

		return flipper;
	}
}
