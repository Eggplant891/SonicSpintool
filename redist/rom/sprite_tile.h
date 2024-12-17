#pragma once

#include "SDL3/SDL_stdinc.h"
#include <vector>

struct SDL_Surface;
namespace spintool
{
	struct BoundingBox;
}

namespace spintool::rom
{
	struct SpriteTile
	{
		Sint16 x_offset = 0;
		Sint16 y_offset = 0;

		Uint8 x_size = 0;
		Uint8 y_size = 0;
		Uint8 padding_0 = 0; // For padding, not real data
		Uint8 padding_1 = 0; // For padding, not real data

		size_t rom_offset = 0;
		size_t rom_offset_end = 0;
		std::vector<Uint32> pixel_data;

		void RenderToSurface(SDL_Surface* surface) const;
		void BlitPixelDataToSurface(SDL_Surface* surface, const BoundingBox& bounds, const std::vector<Uint32>& pixels_data) const;
	};
}