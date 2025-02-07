#pragma once

#include "rom_data.h"
#include "types/decompression_result.h"

#include "SDL3/SDL_stdinc.h"
#include <vector>
#include <optional>
#include <string>

namespace spintool::rom
{
	using SecondDecompressionResult = DecompressionResult;

	class SecondDecompressor
	{
	public:
		static SecondDecompressionResult DecompressData(const std::vector<Uint8>& in_data, const size_t offset);
		static SecondDecompressionResult DecompressDataRefactored(const std::vector<Uint8>& in_data, const size_t offset);
		static SecondDecompressionResult IsValidSSCCompressedData(const Uint8* in_data, const size_t starting_offset);
	private:
	};
}