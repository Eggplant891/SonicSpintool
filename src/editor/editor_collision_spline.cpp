#include "editor/editor_collision_spline.h"

#include "nlohmann/json.hpp"
#include "rom/culling_tables/spline_culling_table.h"

namespace spintool::editor
{
	bool CollisionSpline::operator==(const CollisionSpline& rhs) const
	{
		return min_x == rhs.min_x && min_y == rhs.min_y
			&& max_x == rhs.max_x && max_y == rhs.max_y
			&& object_type_flags == rhs.object_type_flags
			&& instance_id_binding == rhs.instance_id_binding;
	}

	nlohmann::json CollisionSpline::SerialiseToJSON() const
	{
		nlohmann::json spline_json;
		spline_json["min_x"] = min_x;
		spline_json["min_y"] = min_y;
		spline_json["max_x"] = max_x;
		spline_json["max_y"] = max_y;
		spline_json["object_type_flags"] = object_type_flags;
		spline_json["instance_id_binding"] = instance_id_binding;

		return spline_json;
	}

	CollisionSpline CollisionSpline::DeserialiseFromJSON(const nlohmann::json& spline_context)
	{
		CollisionSpline spline;
		spline.min_x = spline_context.at("min_x");
		spline.min_y = spline_context.at("min_y");
		spline.max_x = spline_context.at("max_x");
		spline.max_y = spline_context.at("max_y");
		spline.object_type_flags = spline_context.at("object_type_flags");
		spline.instance_id_binding = spline_context.at("instance_id_binding");

		return spline;
	}

	CollisionSpline CollisionSpline::FromROM(const rom::CollisionSpline& rom_spline)
	{
		CollisionSpline spline;
		spline.min_x = rom_spline.spline_vector.min.x;
		spline.min_y = rom_spline.spline_vector.min.y;
		spline.max_x = rom_spline.spline_vector.max.x;
		spline.max_y = rom_spline.spline_vector.max.y;
		spline.object_type_flags = rom_spline.object_type_flags;
		spline.instance_id_binding = rom_spline.instance_id_binding;

		return spline;
	}
}
