#include "rom/colour.h"

namespace spintool::rom
{
	std::array<Uint8, 17> Colour::levels_lookup =
	{
		  0,
		 29,
		 52,
		 70,
		 87,
		101,
		116,
		130,
		144,
		158,
		172,
		187,
		206,
		228,
		255
	};

	Uint32 Colour::GetPackedU32() const
	{
		return (r << 24) | (g << 16) | (b << 8) | x;
	}

}