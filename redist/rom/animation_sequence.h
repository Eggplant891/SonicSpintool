#pragma once

#include "rom_data.h"

#include <vector>

namespace spintool::rom
{
	struct AnimationSequence
	{
		ROMData rom_data;
	};

	struct AnimationTable
	{
		std::vector<AnimationSequence> animations;
	};
}