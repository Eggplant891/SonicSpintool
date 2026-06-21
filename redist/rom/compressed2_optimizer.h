#pragma once

#include "SDL3/SDL_stdinc.h"

#include <cstddef>
#include <string>
#include <vector>

namespace spintool::rom
{
	struct Compressed2CompressionResult
	{
		std::vector<Uint8> data;
		std::string strategy;
		std::size_t baseline_size = 0;
		std::size_t candidates_tested = 0;
	};

	class Compressed2Optimizer
	{
	public:
		// Compresses a Compressed2/LZW payload using several fully compatible
		// dictionary-reset strategies and returns the smallest valid stream.
		// Supplying the original stream also lets the optimiser reuse its exact
		// reset layout, and preserves it byte-for-byte when the payload is unchanged.
		static Compressed2CompressionResult Compress(
			const std::vector<Uint8>& source,
			const std::vector<Uint8>& original_stream = {}
		);

		// Independent decoder used both for validation and for measuring the exact
		// byte length of an existing bit-packed stream. consumed_size is rounded up
		// to include the final partial byte containing the END token.
		static bool Decode(
			const std::vector<Uint8>& input,
			std::size_t offset,
			std::vector<Uint8>& output,
			std::string& error,
			std::size_t* consumed_size = nullptr,
			std::vector<std::size_t>* reset_output_positions = nullptr
		);
	};
}
