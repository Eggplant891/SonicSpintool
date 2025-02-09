#include "rom/boogaloo_decompressor.h"

#include "SDL3/SDL_stdinc.h"
#include <array>

namespace spintool::rom
{
	std::optional<BoogalooDecompressionResult> PushByteToStack(std::vector<Uint8>& stack_data, Uint8 byte)
	{
		stack_data.emplace_back(byte);

		if (stack_data.size() > 0xFFFF)
		{
			BoogalooDecompressionResult results;
			results.error_msg = "Ran out of memory when writing stack_data";
			results.uncompressed_data = stack_data;
			results.uncompressed_size = results.uncompressed_data.size();

			return results;
		}

		return std::nullopt;
	}

	BoogalooDecompressionResult BoogalooDecompressor::DecompressData(const std::vector<Uint8>& in_data, const size_t offset)
	{
		const size_t start_offset = 0;

		const Uint8* compressed_data = &in_data[offset];

		std::array<Uint16, 4> TokenBitmaskLookup = {
			0x01FF,
			0x03FF,
			0x07FF,
			0x0FFF
		};

		std::vector<Uint8> vram;

		std::vector<Uint8> ram_pool;
		ram_pool.resize(0x10000);

		// ---------------------------------------------------------------------- -
		// INPUT:
		//       a0 - Target VRAM address
		//       a1 - Compressed data pointer
		// ---------------------------------------------------------------------- -

//DoLoadCompressed2Tiles: // CODE XREF : LoadUncOrComp2Tiles + 16
		// LoadCompressed2Tiles + 10↑p
		Uint32 d0 = 0x0;
		Uint32 d1 = 0x0;
		Sint32 d2 = 0x0;
		Uint32 d3 = 0x0; // D3
		Uint32 d4 = 0x102;
		Uint32 d5 = 0x9; // set token size to 9 bits. D5
		Uint32 d6 = 0x200; // set token size to 9 bits
		Uint32 d7 = 0x0;

		Uint32 a0 = 0; // Address to write in VRAM
		Uint32 a1 = static_cast<Uint32>(offset); // Compressed data
		Uint32 a2 = 0x9BCD2; // = TokenBitmaskLookup[0]; // set mask to 9 bits. A2
		Uint32 a3 = 0;
		Uint32 a4 = 0;
		Uint32 a5 = 0x54CC; // A5 points to a table of size $400 bytes(likely)
		Uint32 a6 = 0x58D4; // A5 and A6 point to the same location, but A5 has some base displacement($408 bytes)

		Uint32 sp = 0xFF7C;

		vram.emplace_back(0);
		vram.emplace_back(0);

		a0 = a0 - 2; // subq.w  #2, a0 // -2, because programmer's drunk

		// Setup writes at VRAM address specified by A0
		// jsr     VRAMReadOne // read from `start_address-2`, because programmer's drunk
		// jsr     VRAMWriteOne // write what we've read to `start_address-2`, so the next write goes to `start_address`, because programmer's drunk

		a0 = a1;					// movea.l a1, a0
		sp = sp - 2;				// subq.w  #2, sp
		a1 = sp;					// movea.l sp, a1

	//Decomp_Loop:  // CODE XREF : DoLoadCompressed2Tiles + AE
		while (true)
		{
			// DoLoadCompressed2Tiles + B6 ...
			// Read next token of length 9, 10 or 11 bits(see D5, (A2)) from the compressed stream
			d1 = d3; // d1 = X
			d1 = d1 >> 3; // d1 = X / 8
			d0 = (static_cast<Uint32>(in_data[a0 + d1 + 2]) << 16) | (static_cast<Uint32>(in_data[a0 + d1 + 1]) << 8) | (static_cast<Uint32>(in_data[a0 + d1])); // Read 3 bytes. Reverse order in  binary
			d1 = d3;
			d3 = d3 + d5; // increment compressed stream pointer(in bits)
			d1 = (d1 & 0xFFFF0000) | ((d1 & 0x0000FFFF) & 7);
			d0 = d0 >> d1;
			{
				Uint32 mask = static_cast<Uint32>(((in_data[a2] << 8) | (in_data[a2 + 1])) & 0x0000FFFF);
				d0 = (d0 & 0xFFFF0000) | ((d0 & 0x0000FFFF) & mask); // mask out bits(9, 10 or 11)
			}

			if ((d0 & 0x0000FFFF) == 0x101) // is token $101 ?
			{
				BoogalooDecompressionResult results;
				results.uncompressed_data = vram;
				results.rom_data.SetROMData(offset, a0 + (d3 >> 3));
				results.uncompressed_size = results.uncompressed_data.size();

				return results; // if yes, halt decompression
			}

			if ((d0 & 0x0000FFFF) == 0x100) // is token $100 ?
			{
				// Token $100 : Reset decompression state
				d4 = (d4 & 0xFFFF0000) | (0x102 & 0x0000FFFF);
				d5 = 0x9; // reset token size to 9 bits
				d6 = (d6 & 0xFFFF0000) | (0x200 & 0x0000FFFF);
				a2 = 0x9BCD2; // reset mask to 9 bits
				a6 = 0x58D4; // reset unknown dictionary

				// Read next token of length 9 bits(see D5, (A2)) from the compressed stream
				d1 = d3; // d1 = X
				d1 = d1 >> 3; // d1 = X / 8
				d0 = (static_cast<Uint32>(in_data[a0 + (d1 & 0x0000FFFF) + 2]) << 16) | (static_cast<Uint32>(in_data[a0 + (d1 & 0x0000FFFF) + 1]) << 8) | (static_cast<Uint32>(in_data[a0 + (d1 & 0x0000FFFF)])); // Read 3 bytes. Reverse order in  binary
				d1 = d3;
				d3 = d3 + d5; // increment compressed stream pointer(in bits)
				d1 = (d1 & 0xFFFF0000) | ((d1 & 0x0000FFFF) & 7);
				d0 = d0 >> d1;
				{
					Uint32 mask = static_cast<Uint32>(((in_data[a2] << 8) | (in_data[a2 + 1])) & 0x0000FFFF);
					d0 = (d0 & 0xFFFF0000) | ((d0 & 0x0000FFFF) & mask); // mask out bits(9, 10 or 11)
				}
				d7 = (d7 & 0xFFFF0000) | (d0 & 0x0000FFFF);
				a3 = (a3 & 0xFFFF0000) | (d0 & 0x0000FFFF);
				ram_pool[a1] = (d0 & 0x000000FF); a1 = a1 + 1; // put raw uncompressed byte
				d0 = (d0 & 0xFFFF0000) | (a1 & 0x0000FFFF);

				if ((d0 & 1) != 1)
				{
					a1 = a1 - 2;
					vram.emplace_back(ram_pool[a1]);
					vram.emplace_back(ram_pool[a1+1]);
				}
				continue;
			}

			a4 = (a4 & 0xFFFF0000) | (d0 & 0x0000FFFF);
			if (static_cast<Uint16>(d0 & 0x0000FFFF) < d4)
			{
				goto loc_F5B4C;
			}
			d0 = (d0 & 0xFFFF0000) | (a3 & 0x0000FFFF);
			sp = sp - 2;
			ram_pool[sp] = (d0 & 0x000000FF);
			d0 = (d0 & 0xFFFF0000) | (d7 & 0x0000FFFF);

loc_F5B4A:  // CODE XREF : DoLoadCompressed2Tiles + E4
			d2 = (d2 & 0xFFFF0000) | ((d2 + 1) & 0x0000FFFF); // increment copy counter

loc_F5B4C:  // CODE XREF : DoLoadCompressed2Tiles + C8
			if ((d0 & 0x0000FFFF) > 255)
			{
				d0 = (d0 & 0xFFFF0000) | ((d0 + d0) & 0x0000FFFF); // records in table A5 are 4 bytes long
				d0 = (d0 & 0xFFFF0000) | ((d0 + d0) & 0x0000FFFF);
				sp = sp - 2;
				ram_pool[sp] = ram_pool[a5 + (d0 & 0x0000FFFF) + 1]; // put raw uncompressed byte
				d0 = (d0 & 0xFFFF0000) | (static_cast<Uint32>(ram_pool[a5 + (d0 & 0x0000FFFF) + 2]) << 8) | (ram_pool[a5 + (d0 & 0x0000FFFF) + 3]); // next index or token to process
				goto loc_F5B4A;
			}
			// -------------------------------------------------------------------------- -

//loc_F5B60: // CODE XREF : DoLoadCompressed2Tiles + D6
			a3 = (a3 & 0xFFFF0000) | (d0 & 0x0000FFFF);
			d7 = ((d7 & 0x0000FFFF) << 16) | ((d7 & 0xFFFF0000) >> 16);
			d7 = (d7 & 0xFFFF0000) | (d0 & 0x0000FFFF); // prepare uncompressed byte for unknown dictionary
			d7 = ((d7 & 0x0000FFFF) << 16) | ((d7 & 0xFFFF0000) >> 16); // store it so it's read back by 1(a5,d0.w) (see above)
			sp = sp - 2;
			ram_pool[sp] = (d0 & 0x000000FF); // put raw uncompressed byte

loc_F5B6A:  // CODE XREF : DoLoadCompressed2Tiles:loc_F5B7A
			ram_pool[a1] = ram_pool[sp];
			sp = sp + 2;
			a1 = a1 + 1;

			d0 = (d0 & 0xFFFF0000) | (a1 & 0x0000FFFF);

			if ((d0 & 1) != 1)
			{
				a1 = a1 - 2;
				vram.emplace_back(ram_pool[a1]);
				vram.emplace_back(ram_pool[a1 + 1]);
			}

			d2 = d2 - 1;
			if (d2 >= 0)
			{
				goto loc_F5B6A;
			}

			d2 = 0;
			ram_pool[a6] = (d7 & 0xFF000000) >> 24; // write 4 bytes to unknown dictionary :
			ram_pool[a6+1] = (d7 & 0x00FF0000) >> 16;
			ram_pool[a6+2] = (d7 & 0x0000FF00) >> 8;
			ram_pool[a6+3] = (d7 & 0x000000FF);
			a6 = a6 + 4;
			d4 = (d4 & 0xFFFF0000) | ((d4 + 1) & 0x0000FFFF);			// $00.b - always $00(ignored)
			d7 = (d7 & 0xFFFF0000) | (a4 & 0x0000FFFF);					// $01.b - read back via 1(a5, d0.w) --uncompressed byte
																		// $02.w - read back via 2(a5, d0.w) --next index in table or token to process
			if ((d4 & 0x0000FFFF) < (d6 & 0x0000FFFF))
			{
				continue;
			}

			if ((d5 & 0x0000FFFF) == (0xB & 0x0000FFFF)) // is current token size 11 bits already ? 
			{
				continue;// if yes, back to the loop
			}
			d5 = (d5 & 0xFFFF0000) | ((d5 + 1) & 0x0000FFFF); // increase token size by 1 bit(9->10 bits, 10->11 bits)
			d6 = (d6 & 0xFFFF0000) | ((d6 + d6) & 0x0000FFFF);
			a2 = a2 + 2; // use next bit mask(for 10 or 11 bits).
			continue;
			// End of function DoLoadCompressed2Tiles
		}
	}

	BoogalooDecompressionResult BoogalooDecompressor::DecompressDataRefactored(const std::vector<Uint8>& in_data, const size_t offset)
	{
		BoogalooDecompressionResult final_result;

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

		Uint32 combined_d0 = 0;
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
		token_data.resize(0x400 * 4);

		while (true)
		{
			// Read next token of length 9, 10 or 11 bits(see D5, (A2)) from the compressed stream
			stream_current_offset_bytes = stream_current_offset_bits_d3 >> 3; // X / 8
			upper_d0 = (static_cast<Uint16>(stream_target[stream_current_offset_bytes + 2]));
			lower_d0 = (static_cast<Uint16>(stream_target[stream_current_offset_bytes + 1]) << 8) | (static_cast<Uint16>(stream_target[stream_current_offset_bytes])); // Read 3 bytes. Reverse order in  binary
			stream_current_offset_bits_d3 += token_size_d5; // increment compressed stream pointer(in bits)
			lower_d1 = (stream_current_offset_bits_d3 & 0x0000FFFF) & 7;
			combined_d0 = (upper_d0 << 16) | lower_d0;
			combined_d0 = combined_d0 >> lower_d1;
			lower_d0 = combined_d0 & 0x0000FFFF;
			upper_d0 = (combined_d0 & 0xFFFF0000) >> 16;

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
				//token_data.clear();
				//token_data.resize(0x400 * 4);

				// Read next token of length 9 bits(see D5, (A2)) from the compressed stream
				stream_current_offset_bytes = stream_current_offset_bits_d3 >> 3; // X / 8
				upper_d0 = (static_cast<Uint16>(stream_target[stream_current_offset_bytes + 2]));
				lower_d0 = (static_cast<Uint16>(stream_target[stream_current_offset_bytes + 1]) << 8) | (static_cast<Uint16>(stream_target[stream_current_offset_bytes])); // Read 3 bytes. Reverse order in  binary
				stream_current_offset_bits_d3 += token_size_d5; // increment compressed stream pointer(in bits)
				lower_d1 = (stream_current_offset_bits_d3 & 0x0000FFFF) & 7;

				combined_d0 = (upper_d0 << 16) | lower_d0;
				combined_d0 = combined_d0 >> lower_d1;
				upper_d0 = combined_d0 & 0x0000FFFF;
				lower_d0 = (combined_d0 & 0xFFFF0000) >> 16;

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

			while (lower_d0 > 255)
			{
				const Uint16 index = lower_d0 * 4;
				const Uint8 byte_to_push = (index + 1) >= token_data.size() ? 0 : token_data[index + 1];
				const Uint8 new_index_low = (index + 3) >= token_data.size() ? 0 : token_data[index + 3];
				const Uint8 new_index_high = (index + 2) >= token_data.size() ? 0 : token_data[index + 2];
				auto result = PushByteToStack(stack_data, byte_to_push); // put raw uncompressed byte

				lower_d0 = (static_cast<Uint16>(new_index_high) << 8) | new_index_low; // next index or token to process

				if (result)
				{
					final_result = result.value();
					break;
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