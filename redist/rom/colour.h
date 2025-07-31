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

		Uint32 GetPackedU32() const;

		static std::array<Uint8, 15> levels_lookup;
	};
}