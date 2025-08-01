#pragma once
#include "SDL3/SDL_stdinc.h"

#include <array>
#include <memory>

namespace spintool::rom
{
	using Ptr32 = Uint32;

	struct Palette;
	constexpr static const Uint32 s_max_palettes = 4;
	using PaletteSetArray = std::array<std::shared_ptr<Palette>, s_max_palettes>;

}