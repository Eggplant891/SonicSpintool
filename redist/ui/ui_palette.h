#pragma once

#include "render.h"
#include "types/sdl_handle_defs.h"
#include "rom/spinball_rom.h"
#include "rom/palette.h"

namespace spintool
{
	struct UIPalette
	{
		UIPalette(const rom::Palette& palette);
		SDLPaletteHandle sdl_palette;
		rom::Palette palette;
	};
}
