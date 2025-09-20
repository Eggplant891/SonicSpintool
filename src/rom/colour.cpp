#include "rom/colour.h"

namespace spintool::rom
{
	std::array<Uint8, 16> Colour::levels_lookup =
	{
		  0, // 0
		 29, // 1
		 52, // 2
		 70, // 3
		 87, // 4
		101, // 5
		116, // 6
		130, // 7
		144, // 8
		158, // 9
		172, // A
		187, // B
		206, // C
		228, // D
		255, // E
		255  // F
	};

	Uint32 Colour::GetPackedU32() const
	{
		return (r << 24) | (g << 16) | (b << 8) | x;
	}

}