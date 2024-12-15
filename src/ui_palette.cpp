#include "ui_palette.h"

namespace spintool
{
	UIPalette::UIPalette(const VDPPalette& pal)
		: sdl_palette(Renderer::CreateSDLPalette(pal))
		, palette(pal)
	{

	}
}