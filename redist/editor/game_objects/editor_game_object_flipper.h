#pragma once

#include "SDL3/SDL_stdinc.h"
#include "nlohmann/json_fwd.hpp"

namespace spintool::rom
{
	struct FlipperInstance;
}

namespace spintool::editor
{
	struct FlipperInstance
	{
		Uint16 x_pos = 0;
		Uint16 y_pos = 0;
		bool is_x_flipped = false;

		[[nodiscard]] nlohmann::json SerialiseToJSON() const;
		[[nodiscard]] static FlipperInstance DeserialiseFromJSON(const nlohmann::json& flipper_context);
		[[nodiscard]] static FlipperInstance FromROM(const rom::FlipperInstance& rom_flipper);
	};
}
