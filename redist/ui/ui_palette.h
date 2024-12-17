#pragma once

#include "render.h"
#include "sdl_handle_defs.h"
#include "rom/spinball_rom.h"

namespace spintool
{
	struct UIPalette
	{
		UIPalette(const VDPPalette& palette);
		SDLPaletteHandle sdl_palette;
		VDPPalette palette;
	};
}
