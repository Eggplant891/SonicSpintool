#include "rom/boogaloo_decompressor.h"

#include "SDL3/SDL_stdinc.h"
#include <array>

namespace spintool::rom
{
	std::optional<LZSSDecompressionResult> PushByteToStack(std::vector<Uint8>& stack_data, Uint8 byte)
	{
		stack_data.emplace_back(byte);

		if (stack_data.size() > 0xFFFF)
		{
			LZSSDecompressionResult results;
			results.error_msg = "Ran out of memory when writing stack_data";
			results.uncompressed_data = stack_data;
			results.uncompressed_size = results.uncompressed_data.size();

			return results;
		}

		return std::nullopt;
	}

	LZSSDecompressionResult LZSSDecompressor::DecompressData(const std::vector<Uint8>& in_data, const Uint32 offset)
	{
		const Uint32 start_offset = 0;

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
				LZSSDecompressionResult results;
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

	LZSSDecompressionResult LZSSDecompressor::DecompressDataRefactored(const std::vector<Uint8>& in_data, const Uint32 offset)
	{
		const Uint32 start_offset = 0;

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

		Uint32 d0 = 0x0;
		Uint32 d1 = 0x0;
		Sint16 shorts_to_push = 0x0;
		Uint32 current_bit = 0x0; // D3
		Uint16 d4 = 0x102;
		Uint32 token_size_bits = 0x9; // set token size to 9 bits. D5
		Uint16 d6 = 0x200; // set token size to 9 bits
		Uint32 d7 = 0x0;

		Uint32 sp = 0xFF7C - 2;

		Uint32 a0 = static_cast<Uint32>(offset); // Address to write in VRAM
		Uint32 a1 = sp; // Compressed data
		Uint32 token_bitmask = 0; // = TokenBitmaskLookup[0]; // set mask to 9 bits. A2
		Uint32 a3 = 0;
		Uint16 a4 = 0;
		Uint32 a5 = 0x54CC; // A5 points to a table of size $400 bytes(likely)
		Uint32 dictionary_write_offset = 0x58D4; // A5 and A6 point to the same location, but A5 has some base displacement($408 bytes)


		vram.emplace_back(0);
		vram.emplace_back(0);

		while (true)
		{
			// Read next token of length 9, 10 or 11 bits(see D5, (A2)) from the compressed stream
			d1 = current_bit / 8;
			d0 = (static_cast<Uint32>(in_data[a0 + d1 + 2]) << 16) | (static_cast<Uint32>(in_data[a0 + d1 + 1]) << 8) | (static_cast<Uint32>(in_data[a0 + d1])); // Read 3 bytes. Reverse order in  binary
			d1 = current_bit;
			d1 = (current_bit & 0xFFFF0000) | ((current_bit & 0x0000FFFF) & 7);
			current_bit += token_size_bits; // increment compressed stream pointer(in bits)
			d0 >>= d1;
			d0 = (d0 & 0xFFFF0000) | ((d0 & 0x0000FFFF) & TokenBitmaskLookup[token_bitmask]); // mask out bits(9, 10 or 11)

			if ((d0 & 0x0000FFFF) == 0x101) // Token $100 : Halt decompression
			{
				LZSSDecompressionResult results;
				results.uncompressed_data = vram;
				results.rom_data.SetROMData(offset, a0 + (current_bit >> 3));
				results.uncompressed_size = results.uncompressed_data.size();

				return results;
			}

			if ((d0 & 0x0000FFFF) == 0x100) // Token $100 : Reset decompression state
			{
				
				d4 = (d4 & 0xFFFF0000) | (0x102 & 0x0000FFFF);
				token_size_bits = 0x9; // reset token size to 9 bits
				d6 = 0x200;
				token_bitmask = 0; // reset mask to 9 bits
				dictionary_write_offset = 0x58D4; // reset unknown dictionary

				// Read next token of length 9 bits(see D5, (A2)) from the compressed stream
				d1 = current_bit / 8;
				d0 = (static_cast<Uint32>(in_data[a0 + d1 + 2]) << 16) | (static_cast<Uint32>(in_data[a0 + d1 + 1]) << 8) | (static_cast<Uint32>(in_data[a0 + d1])); // Read 3 bytes. Reverse order in  binary
				d1 = current_bit;
				d1 = (current_bit & 0xFFFF0000) | ((current_bit & 0x0000FFFF) & 7);
				current_bit += token_size_bits; // increment compressed stream pointer(in bits)
				d0 >>= d1;
				d0 = (d0 & 0xFFFF0000) | ((d0 & 0x0000FFFF) & TokenBitmaskLookup[token_bitmask]); // mask out bits(9, 10 or 11)

				d7 = (d7 & 0xFFFF0000) | (d0 & 0x0000FFFF);
				a3 = (a3 & 0xFFFF0000) | (d0 & 0x0000FFFF);
				ram_pool[a1] = (d0 & 0x000000FF); a1 = a1 + 1; // put raw uncompressed byte
				d0 = (d0 & 0xFFFF0000) | (a1 & 0x0000FFFF);

				if ((d0 & 1) != 1)
				{
					a1 = a1 - 2;
					vram.emplace_back(ram_pool[a1]);
					vram.emplace_back(ram_pool[a1 + 1]);
				}
				continue;
			}

			a4 = d0 & 0x0000FFFF;
			if (static_cast<Uint16>(d0 & 0x0000FFFF) < d4)
			{
				goto loc_F5B4C;
			}
			d0 = (d0 & 0xFFFF0000) | (a3 & 0x0000FFFF);
			sp = sp - 2;
			ram_pool[sp] = (d0 & 0x000000FF);
			d0 = (d0 & 0xFFFF0000) | (d7 & 0x0000FFFF);

		loc_F5B4A:  // CODE XREF : DoLoadCompressed2Tiles + E4
			shorts_to_push = (shorts_to_push & 0xFFFF0000) | ((shorts_to_push + 1) & 0x0000FFFF); // increment copy counter

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

			for (; shorts_to_push >= 0; --shorts_to_push)
			{
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
			}
			shorts_to_push = 0;
			ram_pool[dictionary_write_offset] = (d7 & 0xFF000000) >> 24; // write 4 bytes to unknown dictionary :
			ram_pool[dictionary_write_offset + 1] = (d7 & 0x00FF0000) >> 16;
			ram_pool[dictionary_write_offset + 2] = (d7 & 0x0000FF00) >> 8;
			ram_pool[dictionary_write_offset + 3] = (d7 & 0x000000FF);
			dictionary_write_offset = dictionary_write_offset + 4;
			++d4;														// $00.b - always $00(ignored)
			d7 = (d7 & 0xFFFF0000) | a4;					// $01.b - read back via 1(a5, d0.w) --uncompressed byte
																		// $02.w - read back via 2(a5, d0.w) --next index in table or token to process
			if (d4 < d6)
			{
				continue;
			}

			if ((token_size_bits & 0x0000FFFF) == (0xB & 0x0000FFFF)) // is current token size 11 bits already ? 
			{
				continue;// if yes, back to the loop
			}
			token_size_bits = (token_size_bits & 0xFFFF0000) | ((token_size_bits + 1) & 0x0000FFFF); // increase token size by 1 bit(9->10 bits, 10->11 bits)
			d6 += d6;
			++token_bitmask; // use next bit mask(for 10 or 11 bits).
			
			continue;
			// End of function DoLoadCompressed2Tiles
		}
	}
}