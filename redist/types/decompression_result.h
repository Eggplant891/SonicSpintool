#pragma once

#include "rom/rom_data.h"

#include "SDL3/SDL_stdinc.h"

#include <vector>
#include <optional>
#include <string>

namespace spintool
{
	enum class CompressionAlgorithm
	{
		NONE,
		SSC,
		LZSS
	};

	struct DecompressionResult
	{
		rom::ROMData rom_data;
		std::vector<Uint8> uncompressed_data;
		std::optional<std::string> error_msg;

		size_t uncompressed_size = 0;

		bool operator!=(const DecompressionResult& rhs) const
		{
			return operator==(rhs) == false;
		}
		bool operator==(const DecompressionResult& rhs) const
		{
			return rom_data == rhs.rom_data
				&& uncompressed_data == rhs.uncompressed_data
				&& error_msg == rhs.error_msg
				&& uncompressed_size == rhs.uncompressed_size;
		}
	};
}