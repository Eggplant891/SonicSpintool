#include "rom/second_decompressor.h"

#include "SDL3/SDL_stdinc.h"
#include <array>

namespace spintool::rom
{
	SecondDecompressionResult SecondDecompressor::DecompressData(const std::vector<Uint8>& in_data, const size_t offset)
	{
		const size_t start_offset = 0;

		const Uint8* stream_target = &in_data[offset];

		std::array<Uint16, 4> TokenBitmaskLookup = {
			0x01FF,
			0x03FF,
			0x07FF,
			0x0FFF
		};

		std::vector<Uint8> out_data;
		std::vector<Uint8> stack_data;

		// ---------------------------------------------------------------------- -
		// INPUT:
		//       a0 - Target VRAM address
		//       a1 - Compressed data pointer
		// ---------------------------------------------------------------------- -

//DoLoadCompressed2Tiles: // CODE XREF : LoadUncOrComp2Tiles + 16
		// LoadCompressed2Tiles + 10↑p
		Uint32 d0 = 0;
		Uint32 d1 = 0;
		Uint32 copy_counter_d2 = 0;
		Uint32 stream_current_offset_d3 = 0; // D3
		Sint16 working_d4 = 0x102;
		Uint32 token_size_d5 = 9; // set token size to 9 bits. D5
		Sint16 working_d6 = 0x200; // set token size to 9 bits
		Uint32 d7 = 0;

		Uint16 loaded_bitmask_index_a2 = 0; // = TokenBitmaskLookup[0]; // set mask to 9 bits. A2
		Uint32 d0_stored_in_a3 = 0;
		size_t offset_a5 = start_offset; // A5 points to a table of size $400 bytes(likely)
		size_t offset_a6 = start_offset + 0x408; // A5 and A6 point to the same location, but A5 has some base displacement($408 bytes)

		// Setup writes at VRAM address specified by A0
		// subq.w  #2, a0 // -2, because programmer's drunk
		// jsr     VRAMReadOne // read from `start_address-2`, because programmer's drunk
		// jsr     VRAMWriteOne // write what we've read to `start_address-2`, so the next write goes to `start_address`, because programmer's drunk
		// movea.l a1, a0
		// subq.w  #2, sp
		// movea.l sp, a1

	//Decomp_Loop:  // CODE XREF : DoLoadCompressed2Tiles + AE
		while (true)
		{
			// DoLoadCompressed2Tiles + B6 ...
			// Read next token of length 9, 10 or 11 bits(see D5, (A2)) from the compressed stream
			d1 = stream_current_offset_d3; // d1 = X
			d1 = d1 >> 3; // d1 = X / 8
			d0 = (static_cast<Uint32>(stream_target[d1 + 2]) << 16) | (static_cast<Uint32>(stream_target[d1 + 1]) << 8) | (static_cast<Uint32>(stream_target[d1])); // Read 3 bytes. Reverse order in  binary
			d1 = stream_current_offset_d3;
			stream_current_offset_d3 += token_size_d5; // increment compressed stream pointer(in bits)
			d1 = (d1 & 0xFFFF0000) | ((d1 & 0x0000FFFF) & 7);
			d0 = d0 >> d1;
			d0 = (d0 & 0xFFFF0000) | ((d0 & 0x0000FFFF) & TokenBitmaskLookup[loaded_bitmask_index_a2]); // mask out bits(9, 10 or 11)

			if ((d0 & 0x0000FFFF) == 0x101) // is token $101 ?
			{
				SecondDecompressionResult results;
				results.uncompressed_data = stack_data;
				results.rom_data.SetROMData(&in_data[offset], stream_target + 1, offset);
				results.uncompressed_size = results.uncompressed_data.size();

				return results; // if yes, halt decompression
			}

			if ((d0 & 0x0000FFFF) == 0x100) // is token $100 ?
			{
				// Token $100 : Reset decompression state
				working_d4 = 0x102;
				token_size_d5 = 9; // reset token size to 9 bits
				working_d6 = 0x200;
				loaded_bitmask_index_a2 = 0; // reset mask to 9 bits
				offset_a6 = start_offset + 0x408; // reset unknown dictionary

				// Read next token of length 9 bits(see D5, (A2)) from the compressed stream
				d1 = stream_current_offset_d3; // d1 = X
				d1 = d1 >> 3; // d1 = X / 8
				d0 = (static_cast<Uint32>(stream_target[d1 + 2]) << 16) | (static_cast<Uint32>(stream_target[d1 + 1]) << 8) | (static_cast<Uint32>(stream_target[d1])); // Read 3 bytes. Reverse order in  binary
				d1 = stream_current_offset_d3;
				stream_current_offset_d3 += token_size_d5; // increment compressed stream pointer(in bits)
				d1 = (d1 & 0xFFFF0000) | ((d1 & 0x0000FFFF) & 7);
				d0 = d0 >> d1;
				d0 = (d0 & 0xFFFF0000) | ((d0 & 0x0000FFFF) & TokenBitmaskLookup[loaded_bitmask_index_a2]); // mask out bits(9, 10 or 11)

				d7 = (d0 & 0x0000FFFF);
				d0_stored_in_a3 = (d0 & 0x0000FFFF);
				out_data.emplace_back((d0 & 0x000000FF)); // put raw uncompressed byte
				continue;
			}
			Uint16 a4 = (d0 & 0x0000FFFF);
			if (static_cast<Uint16>(d0 & 0x0000FFFF) < working_d4)
			{
				goto loc_F5B4C;
			}
			d0 = (d0 & 0xFFFF0000) | (d0_stored_in_a3 & 0x0000FFFF);
			stack_data.emplace_back(d0 & 0x000000FF);
			d0 = (d0 & 0xFFFF0000) | (d7 & 0x0000FFFF);

loc_F5B4A:  // CODE XREF : DoLoadCompressed2Tiles + E4
			++copy_counter_d2; // increment copy counter

loc_F5B4C:  // CODE XREF : DoLoadCompressed2Tiles + C8
			if ((d0 & 0x0000FFFF) > 255)
			{
				d0 = (d0 & 0xFFFF0000) | (static_cast<Uint16>(d0) + static_cast<Uint16>(d0) + static_cast<Uint16>(d0) + static_cast<Uint16>(d0)); // records in table A5 are 4 bytes long
				stack_data.emplace_back(stream_target[(d0 & 0x0000FFFF) + 1]); // put raw uncompressed byte
				d0 = stream_target[(d0 & 0x0000FFFF) + 2] & 0x0000FFFF; // next index or token to process
				goto loc_F5B4A;
			}
			// -------------------------------------------------------------------------- -

//loc_F5B60: // CODE XREF : DoLoadCompressed2Tiles + D6
			d0_stored_in_a3 = d0 & 0x0000FFFF;
			d7 = ((d7 & 0x0000FFFF) << 16) | ((d7 & 0xFFFF0000) >> 16);
			d7 = (d7 & 0xFFFF0000) | (d0 & 0x0000FFFF); // prepare uncompressed byte for unknown dictionary
			d7 = ((d7 & 0x0000FFFF) << 16) | ((d7 & 0xFFFF0000) >> 16); // store it so it's read back by 1(a5,d0.w) (see above)
			stack_data.emplace_back(d0 & 0x000000FF); // put raw uncompressed byte

//loc_F5B6A:  // CODE XREF : DoLoadCompressed2Tiles:loc_F5B7A
			//move.b(sp) + , (a1)+ // put raw uncompressed byte
			//move.w  a1, d0 // get stack variable address
			//btst    #0, d0 // are we doing the second byte already ?
			//bne.s   loc_F5B7A // branch if not
			//move.w - (a1), ($C00000).l // dump 2 bytes to VRAM

//loc_F5B7A:  // CODE XREF : DoLoadCompressed2Tiles + F8
			//dbf     d2, loc_F5B6A
			copy_counter_d2 = 0;
			out_data.emplace_back(d7 & 0xFF000000); // write 4 bytes to unknown dictionary :
			out_data.emplace_back(d7 & 0x00FF0000);
			out_data.emplace_back(d7 & 0x0000FF00);
			out_data.emplace_back(d7 & 0x000000FF);
			// $00.b - always $00(ignored)
	   // $01.b - read back via 1(a5, d0.w) --uncompressed byte
	   // $02.w - read back via 2(a5, d0.w) --next index in table or token to process
			++working_d4;
			d7 = (d7 & 0xFFFF0000) | (a4 & 0x0000FFFF);
			if (working_d4 < working_d6)
			{
				continue;
			}

			if (token_size_d5 == 0xB) // is current token size 11 bits already ? 
			{
				continue;// if yes, back to the loop
			}
			++token_size_d5; // increase token size by 1 bit(9->10 bits, 10->11 bits)
			working_d6 = working_d6 + working_d6;
			++loaded_bitmask_index_a2; // use next bit mask(for 10 or 11 bits)
			continue;
			// End of function DoLoadCompressed2Tiles
		}
	}
}