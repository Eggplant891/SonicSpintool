#pragma once

#include "SDL3/SDL_stdinc.h"
#include "nlohmann/json_fwd.hpp"

namespace spintool::rom
{
	struct CollisionSpline;
}

namespace spintool::editor
{
	struct CollisionSpline
	{
		int min_x = 0;
		int min_y = 0;
		int max_x = 0;
		int max_y = 0;
		Uint16 object_type_flags = 0;
		Uint16 instance_id_binding = 0;

		bool operator==(const CollisionSpline& rhs) const;

		[[nodiscard]] nlohmann::json SerialiseToJSON() const;
		[[nodiscard]] static CollisionSpline DeserialiseFromJSON(const nlohmann::json& spline_context);
		[[nodiscard]] static CollisionSpline FromROM(const rom::CollisionSpline& rom_spline);
	};
}
