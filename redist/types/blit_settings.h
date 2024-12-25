#pragma once

#include "rom/palette.h"

#include "SDL3/SDL_stdinc.h"

#include <optional>

namespace spintool
{
	struct BlitSettings
	{
		bool flip_horizontal = false;
		bool flip_vertical = false;
		std::optional<rom::Palette> palette;
	};
}