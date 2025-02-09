#pragma once

#include "rom_data.h"
#include "types/decompression_result.h"

#include "SDL3/SDL_stdinc.h"
#include <vector>
#include <optional>
#include <string>

namespace spintool::rom
{
	using SSCDecompressionResult = DecompressionResult;

	class SSCDecompressor
	{
	public:
		static SSCDecompressionResult DecompressData(const std::vector<Uint8>& in_data, size_t offset, size_t working_data_size_hint);
		static SSCDecompressionResult IsValidSSCCompressedData(const Uint8* in_data, const size_t starting_offset);
	private:
	};
}