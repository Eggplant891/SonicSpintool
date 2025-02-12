#pragma once

#include "rom_data.h"
#include "SDL3/SDL_stdinc.h"

namespace spintool::rom
{
	class SpinballROM;
	struct SpriteTileHeader;
}

namespace spintool::rom
{
	struct CollisionTile
	{
		ROMData tile_rom_data;

		const Uint8* LoadFromROM(const SpriteTileHeader& header, const size_t rom_data_offset, const SpinballROM& src_rom);
	};
}