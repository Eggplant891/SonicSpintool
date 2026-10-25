#include "editor/game_objects/editor_game_object_ring.h"

#include "nlohmann/json.hpp"
#include "rom/game_objects/game_object_ring.h"

namespace spintool::editor
{
	nlohmann::json RingInstance::SerialiseToJSON() const
	{
		nlohmann::json ring_json;
		ring_json["instance_id"] = instance_id;
		ring_json["x_pos"] = x_pos;
		ring_json["y_pos"] = y_pos;

		return ring_json;
	}

	RingInstance RingInstance::DeserialiseFromJSON(const nlohmann::json& ring_context)
	{
		RingInstance ring;
		ring.instance_id = ring_context.at("instance_id");
		ring.x_pos = ring_context.at("x_pos");
		ring.y_pos = ring_context.at("y_pos");

		return ring;
	}

	RingInstance RingInstance::FromROM(const rom::RingInstance& rom_ring)
	{
		RingInstance ring;
		ring.instance_id = rom_ring.instance_id;
		ring.x_pos = rom_ring.x_pos;
		ring.y_pos = rom_ring.y_pos;

		return ring;
	}
}
