#pragma once

#include "SDL3/SDL_stdinc.h"
#include <array>

namespace spintool::rom
{
	struct Colour
	{
		Uint8 x;
		Uint8 b;
		Uint8 g;
		Uint8 r;

		static std::array<Uint8, 17> levels_lookup;
	};
}