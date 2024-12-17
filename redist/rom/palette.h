#pragma once

#include "rom/colour.h"

#include "SDL3/SDL_stdinc.h"
#include <array>

struct ImColor;

namespace spintool::rom
{
	struct Swatch
	{
		Uint16 packed_value;

		Colour GetUnpacked() const;
		static Uint16 Pack(float r, float g, float b);
		ImColor AsImColor() const;
	};

	struct Palette
	{
		std::array<Swatch, 16> palette_swatches;
		size_t offset;
	};
}