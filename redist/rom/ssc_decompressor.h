#pragma once

#include "SDL3/SDL_stdinc.h"
#include <vector>

namespace spintool::rom
{
	class SSCDecompressor
	{
	public:
		static std::vector<Uint8> DecompressData(const Uint8* in_data, size_t size_hint);
	private:
	};
}