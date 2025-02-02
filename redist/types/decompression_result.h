#pragma once

#include "rom/rom_data.h"

#include "SDL3/SDL_stdinc.h"

#include <vector>
#include <optional>
#include <string>

namespace spintool
{
	struct DecompressionResult
	{
		rom::ROMData rom_data;
		std::vector<Uint8> uncompressed_data;
		std::optional<std::string> error_msg;

		size_t uncompressed_size = 0;
	};
}