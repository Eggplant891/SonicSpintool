#pragma once

#include "rom_data.h"
#include "types/decompression_result.h"

#include "SDL3/SDL_stdinc.h"
#include <vector>
#include <optional>
#include <string>

namespace spintool::rom
{
	using BoogalooDecompressionResult = DecompressionResult;

	class BoogalooDecompressor
	{
	public:
		static BoogalooDecompressionResult DecompressData(const std::vector<Uint8>& in_data, const Uint32 offset);
		static BoogalooDecompressionResult DecompressDataRefactored(const std::vector<Uint8>& in_data, const Uint32 offset);
		static BoogalooDecompressionResult IsValidSSCCompressedData(const Uint8* in_data, const Uint32 starting_offset);
	private:
	};
}