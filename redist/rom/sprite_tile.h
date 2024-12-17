#pragma once

#include "rom/rom_data.h"

#include "SDL3/SDL_stdinc.h"
#include <vector>

struct SDL_Surface;
namespace spintool
{
	struct BoundingBox;
}

namespace spintool::rom
{
	struct SpriteTileHeader
	{
		Sint16 x_offset = 0;
		Sint16 y_offset = 0;

		Uint8 x_size = 0;
		Uint8 y_size = 0;
		Uint8 padding_0 = 0; // For padding, not real data
		Uint8 padding_1 = 0; // For padding, not real data

		ROMData header_rom_data;

		const Uint8* LoadFromROM(const Uint8* rom_data_start, const size_t rom_data_offset);
	};

	struct SpriteTileData
	{
		std::vector<Uint32> pixel_data;

		ROMData tile_rom_data;

		const Uint8* LoadFromROM(const SpriteTileHeader& header, const Uint8* rom_data_start, const size_t rom_data_offset);
	};

	struct SpriteTile
		: public SpriteTileHeader
		, public SpriteTileData
	{
		void RenderToSurface(SDL_Surface* surface) const;
		void BlitPixelDataToSurface(SDL_Surface* surface, const BoundingBox& bounds, const std::vector<Uint32>& pixels_data) const;
	};
}