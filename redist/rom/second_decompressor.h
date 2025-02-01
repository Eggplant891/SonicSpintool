#pragma once

#include "rom_data.h"

#include "SDL3/SDL_stdinc.h"
#include <vector>
#include <optional>
#include <string>

namespace spintool::rom
{
	struct SecondDecompressionResult
	{
		ROMData rom_data;
		std::vector<Uint8> uncompressed_data;
		std::optional<std::string> error_msg;

		size_t uncompressed_size = 0;
	};

	class SecondDecompressor
	{
	public:
		static SecondDecompressionResult DecompressData(const std::vector<Uint8>& in_data, const size_t offset);
		static SecondDecompressionResult IsValidSSCCompressedData(const Uint8* in_data, const size_t starting_offset);
	private:
	};
}