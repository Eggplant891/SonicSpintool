#pragma once

#include "types/decompression_result.h"
#include <vector>
#include "SDL3/SDL_stdinc.h"

namespace spintool::rom
{
	using SSCCompressionResult = std::vector<Uint8>;

	class SSCCompressor
	{
	public:
		static SSCCompressionResult CompressData(const std::vector<Uint8>& in_data, Uint32 offset, Uint32 working_data_size_hint);
	};
}