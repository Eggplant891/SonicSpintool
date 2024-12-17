#include "ui/ui_palette.h"

namespace spintool
{
	UIPalette::UIPalette(const rom::Palette& pal)
		: sdl_palette(Renderer::CreateSDLPalette(pal))
		, palette(pal)
	{

	}
}