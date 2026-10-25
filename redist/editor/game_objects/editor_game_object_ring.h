#pragma once

#include "SDL3/SDL_stdinc.h"
#include "nlohmann/json_fwd.hpp"

namespace spintool::rom
{
	struct RingInstance;
}

namespace spintool::editor
{
	struct RingInstance
	{
		Uint16 instance_id = 0;
		Uint16 x_pos = 0;
		Uint16 y_pos = 0;

		[[nodiscard]] nlohmann::json SerialiseToJSON() const;
		[[nodiscard]] static RingInstance DeserialiseFromJSON(const nlohmann::json& ring_context);
		[[nodiscard]] static RingInstance FromROM(const rom::RingInstance& rom_ring);
	};
}
