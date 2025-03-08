#pragma once

#include "rom_data.h"
#include "types/decompression_result.h"

#include "SDL3/SDL_stdinc.h"
#include <vector>
#include <optional>
#include <string>

namespace spintool::rom
{
	using LZSSDecompressionResult = DecompressionResult;

	class LZSSDecompressor
	{
	public:
		static LZSSDecompressionResult DecompressData(const std::vector<Uint8>& in_data, const Uint32 offset);
		static LZSSDecompressionResult DecompressDataRefactored(const std::vector<Uint8>& in_data, const Uint32 offset);
	private:
	};
}