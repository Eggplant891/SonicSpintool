#include "rom/second_decompressor.h"

#include "SDL3/SDL_stdinc.h"
#include <array>

namespace spintool::rom
{
	std::optional<SecondDecompressionResult> PushByteToStack(std::vector<Uint8>& stack_data, Uint8 byte)
	{
		stack_data.emplace_back(byte);

		if (stack_data.size() > 0xFFFF)
		{
			SecondDecompressionResult results;
			results.error_msg = "Ran out of memory when writing stack_data";
			results.uncompressed_data = stack_data;
			results.uncompressed_size = results.uncompressed_data.size();

			return results;
		}

		return std::nullopt;
	}

	SecondDecompressionResult SecondDecompressor::DecompressData(const std::vector<Uint8>& in_data, const size_t offset)
	{
		SecondDecompressionResult final_result;

		const size_t start_offset = 0;
		const Uint8* stream_target = &in_data[offset];

		static const std::array<Uint16, 4> TokenBitmaskLookup = {
			0x01FF,
			0x03FF,
			0x07FF,
			0x0FFF
		};

		std::vector<Uint8> token_data;
		std::vector<Uint8> stack_data;

		constexpr Uint16 s_token_reset_state = 0x100;
		constexpr Uint16 s_token_halt = 0x101;
		constexpr size_t a5_to_a6_offset = 0x408;

		Uint16 upper_d0 = 0;
		Uint16 lower_d0 = 0;
		Uint16 upper_d1 = 0;
		Uint16 lower_d1 = 0;
		Uint32 stream_current_offset_bytes = 0;
		Uint32 stream_current_offset_bits_d3 = 0; // D3
		Uint16 written_token_end_index = 0x102;
		Uint32 token_size_d5 = 9; // set token size to 9 bits. D5
		Uint16 max_token_index = 0x200; // set token size to 9 bits
		Uint16 upper_d7 = 0;
		Uint16 lower_d7 = 0;


		Uint16 loaded_bitmask_index_a2 = 0; // = TokenBitmaskLookup[0]; // set mask to 9 bits. A2
		Uint16 lower_d0_stored_in_a3 = 0;
		size_t offset_a5 = start_offset; // A5 points to a table of size $400 bytes(likely)
		size_t next_token_write = start_offset + a5_to_a6_offset; // A5 points to a table of size $400 bytes(likely) // A5 and A6 point to the same location, but A5 has some base displacement($408 bytes)
		token_data.resize(0x400* 4);

		while (true)
		{
			// Read next token of length 9, 10 or 11 bits(see D5, (A2)) from the compressed stream
			stream_current_offset_bytes = stream_current_offset_bits_d3 >> 3; // X / 8
			upper_d0 = (static_cast<Uint16>(stream_target[stream_current_offset_bytes + 2]));
			lower_d0 = (static_cast<Uint16>(stream_target[stream_current_offset_bytes + 1]) << 8) | (static_cast<Uint32>(stream_target[stream_current_offset_bytes])); // Read 3 bytes. Reverse order in  binary
			stream_current_offset_bits_d3 += token_size_d5; // increment compressed stream pointer(in bits)
			lower_d1 = (stream_current_offset_bits_d3 & 0x0000FFFF) & 7;
			lower_d0 = lower_d0 >> lower_d1;
			upper_d0 = upper_d0 >> lower_d1;
			lower_d0 = lower_d0 & TokenBitmaskLookup[loaded_bitmask_index_a2]; // mask out bits(9, 10 or 11)

			if (lower_d0 == s_token_halt) // Token $101 : Halt decompression
			{
				if (stack_data.empty())
				{
					final_result.error_msg = "No valid data found";
				}
				break;
			}
			else if (lower_d0 == s_token_reset_state) // Token $100 : Reset decompression state
			{
				written_token_end_index = 0x102;
				token_size_d5 = 9; // reset token size to 9 bits
				max_token_index = 0x200;
				loaded_bitmask_index_a2 = 0; // reset mask to 9 bits
				next_token_write = start_offset + a5_to_a6_offset; // reset unknown dictionary

				// Read next token of length 9 bits(see D5, (A2)) from the compressed stream
				stream_current_offset_bytes = stream_current_offset_bits_d3 >> 3; // X / 8
				upper_d0 = (static_cast<Uint16>(stream_target[stream_current_offset_bytes + 2]));
				lower_d0 = (static_cast<Uint16>(stream_target[stream_current_offset_bytes + 1]) << 8) | (static_cast<Uint32>(stream_target[stream_current_offset_bytes])); // Read 3 bytes. Reverse order in  binary
				stream_current_offset_bits_d3 += token_size_d5; // increment compressed stream pointer(in bits)
				lower_d1 = (stream_current_offset_bits_d3 & 0x0000FFFF) & 7;
				lower_d0 = lower_d0 >> lower_d1;
				upper_d0 = upper_d0 >> lower_d1;
				lower_d0 = lower_d0 & TokenBitmaskLookup[loaded_bitmask_index_a2]; // mask out bits(9, 10 or 11)

				lower_d7 = lower_d0;
				lower_d0_stored_in_a3 = lower_d0;
				//token_data.emplace_back((d0 & 0x000000FF)); // put raw uncompressed byte
				continue;
			}

			Uint16 a4 = lower_d0;
			if (lower_d0 >= written_token_end_index)
			{
				auto result = PushByteToStack(stack_data, lower_d0_stored_in_a3 & 0x00FF);
				if (result)
				{
					final_result = result.value();
					break;
				}
				lower_d0 = lower_d7;
			}
			else
			{
				while (lower_d0 > 255)
				{
					const Uint16 index = lower_d0 * 4;
					const Uint8 byte_to_push = (index + 1) >= token_data.size() ? 0 : token_data[index + 1];
					const Uint8 new_index_low = (index + 2) >= token_data.size() ? 0 : token_data[index + 2];
					const Uint8 new_index_high = (index + 3) >= token_data.size() ? 0 : token_data[index + 3];
					auto result = PushByteToStack(stack_data, byte_to_push); // put raw uncompressed byte
					lower_d0 = (static_cast<Uint16>(new_index_high) << 8) | new_index_low; // next index or token to process

					if (result)
					{
						final_result = result.value();
						break;
					}
				}
			}

			lower_d0_stored_in_a3 = lower_d0;
			upper_d7 = lower_d0; // prepare uncompressed byte for unknown dictionary
			auto result = PushByteToStack(stack_data, lower_d0 & 0x00FF); // put raw uncompressed byte
			if (result)
			{
				final_result = result.value();
				break;
			}

			if (next_token_write + 4 > token_data.size()) // Equal to size means we'll still write within bounds this time around
			{
				final_result.error_msg = "Attempt to write outside of token data memory space";
				break;
			}

			token_data[next_token_write] = (upper_d7 & 0xFF00) >> 8;		// Write 4 bytes to unknown dictionary :
			token_data[next_token_write + 1] = upper_d7 & 0x00FF;			// $00.b - always $00(ignored)
			token_data[next_token_write + 2] = (lower_d7 & 0xFF00) >> 8;	// $01.b - read back via 1(a5, d0.w) --uncompressed byte
			token_data[next_token_write + 3] = lower_d7 & 0x00FF;			// $02.w - read back via 2(a5, d0.w) --next index in table or token to process
			next_token_write += 4;

			++written_token_end_index;
			lower_d7 = a4;
			if (written_token_end_index < max_token_index)
			{
				continue;
			}

			if (token_size_d5 == 0xB) // is current token size 11 bits already ? 
			{
				continue;// if yes, back to the loop
			}
			++token_size_d5; // increase token size by 1 bit(9->10 bits, 10->11 bits)
			max_token_index *= 2;
			++loaded_bitmask_index_a2; // use next bit mask(for 10 or 11 bits)
			continue;
			// End of function DoLoadCompressed2Tiles
		}

		// FINISHED - Fill out results with current state, if error occured or not.
		final_result.uncompressed_data = stack_data;
		final_result.rom_data.SetROMData(&in_data[offset], &stream_target[stream_current_offset_bits_d3 >> 3], offset);
		final_result.uncompressed_size = final_result.uncompressed_data.size();

		return final_result;
	}
}