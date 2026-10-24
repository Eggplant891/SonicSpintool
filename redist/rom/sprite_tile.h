#pragma once

#include "rom/rom_data.h"
#include "types/blit_settings.h"

#include "SDL3/SDL_stdinc.h"
#include <vector>
#include <optional>

struct SDL_Surface;
namespace spintool
{
	struct BoundingBox;
	namespace rom
	{
		class SpinballROM;
	}
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

		static constexpr size_t size_on_rom = 6;

		Uint32 LoadFromROM(const SpinballROM& src_rom, Uint32 offset);
	};

	struct SpriteTileData
	{
		std::vector<Uint32> pixel_data;

		ROMData tile_rom_data;

		Uint32 LoadFromROM(const SpriteTileHeader& header, const SpinballROM& src_rom, Uint32 offset);
	};

	struct SpriteTile
		: public SpriteTileHeader
		, public SpriteTileData
	{
		BlitSettings blit_settings;
		// Title-screen object mappings may select one of four palette lines.
		// Other sprite formats leave this at the default line 0.
		Uint8 palette_line = 0;

		size_t SizeOnROM() const;
		
		void RenderToSurface(SDL_Surface* surface) const;
		void BlitPixelDataToSurface(SDL_Surface* surface, const BoundingBox& bounds, const std::vector<Uint32>& pixels_data) const;
	};
}
