#include "rom/ssc_decompressor.h"

namespace spintool::rom
{
	SSCDecompressionResult SSCDecompressor::IsValidSSCCompressedData(const Uint8* in_data, const size_t starting_offset)
	{
		SSCDecompressionResult results;

		const Uint8* current_byte = in_data;
		// Check that first bit of data is considered a raw byte. The first bit of data but always be raw, as there will be no predictable data in the working buffer
		{
			const Uint8 fragment_header = *current_byte;
			const bool is_raw_data = (fragment_header & 1) == 1;
			if (is_raw_data == false)
			{
				results.rom_data.SetROMData(starting_offset, starting_offset + 2);
				results.error_msg = "The first fragment data header bitfield did not start the data fragment with a raw byte";
				return results;
			}
		}

		Uint16 num_bytes_pushed = 0;
		while (true)
		{
			const Uint8* next_byte = nullptr;
			Uint8 fragment_header = *current_byte;
			++current_byte;
			for (int fragments_remaining = 7; fragments_remaining >= 0; --fragments_remaining)
			{
				const bool is_raw_data = (fragment_header & 1) == 1;
				fragment_header = fragment_header >> 1;
				if (is_raw_data == false)
				{
					next_byte = current_byte + 1;
					const Uint16 tile_index_offset = (static_cast<Uint16>((*next_byte) & 0xF0) << 4) | static_cast<Uint16>(*current_byte);

					//if (tile_index_offset > num_bytes_pushed + 0xF)
					//{
					//	results.rom_data.SetROMData(starting_offset, starting_offset + 2);
					//	results.validation_error_msg = "Attempted to read a byte from a working_data address that has not yet been written to.";
					//	return results;
					//}

					if (tile_index_offset == 0)
					{
						if (num_bytes_pushed <= 4)
						{
							results.rom_data.SetROMData(starting_offset, starting_offset + 2);
							results.error_msg = "Did not push more than 4 bytes of data. This completely negates the points of using SSC compression and therefore is likely invalid.";
						}
						else
						{
							results.rom_data.SetROMData(in_data, next_byte+1, starting_offset);
						}
						results.uncompressed_size = num_bytes_pushed;
						return results;
					}
					num_bytes_pushed += (*next_byte & 0xF) + 1;
				}
				else
				{
					num_bytes_pushed += 1;
					next_byte = current_byte;
				}
				current_byte = next_byte + 1;
			}
		}

		results.rom_data.rom_offset_end = starting_offset + 2;
		results.error_msg = "Somehow broke out of while loop. AAAA";

		results.uncompressed_size = num_bytes_pushed;
		results.rom_data.SetROMData(in_data, current_byte, starting_offset);
		return results;
	}

	SSCDecompressionResult SSCDecompressor::DecompressData(const Uint8* in_data, size_t size_hint)
	{
		const Uint8* compressed_data_root = in_data;
		SSCDecompressionResult results;
		std::vector<Uint8>& out_data = results.uncompressed_data;
		static std::vector<Uint8> working_data = []()  // Needs to be indexable even before we have written to it
			{
				std::vector<Uint8> data_vec;
				data_vec.resize(0x1000);
				return data_vec;
			}();

		out_data.reserve(size_hint); // Size needs to be accurate to determine where to index in working_data.
		memset(working_data.data(), 0, 0x1000);

		bool end_reached = false;
		while (end_reached == false)
		{
			const Uint8* next_byte = nullptr;
			Uint8 fragment_header = *compressed_data_root;
			++compressed_data_root;

			for (int fragments_remaining = 7; fragments_remaining >= 0; --fragments_remaining)
			{
				const bool is_raw_data = (fragment_header & 1) == 1;
				fragment_header = fragment_header >> 1;
				if (is_raw_data == false)
				{
					next_byte = compressed_data_root + 1;

					Uint16 tile_index_offset = (static_cast<Uint16>((*next_byte) & 0xF0) << 4) | static_cast<Uint16>(*compressed_data_root);
					if (tile_index_offset == 0)
					{
						end_reached = true;
						break;
					}

					for (int num_bytes_to_print = (*next_byte & 0xF) + 1; num_bytes_to_print >= 0; --num_bytes_to_print)
					{
						const Uint8 value_to_write = working_data.at(tile_index_offset & 0xFFF);
						out_data.emplace_back(value_to_write);
						working_data[out_data.size() & 0xFFF] = value_to_write;
						tile_index_offset = (tile_index_offset + 1) & 0xFFF;
					}
				}
				else
				{
					out_data.emplace_back(*compressed_data_root);
					working_data[out_data.size() & 0xFFF] = *compressed_data_root;
					next_byte = compressed_data_root;
				}
				compressed_data_root = next_byte + 1;
			}
		}

		results.uncompressed_size = results.uncompressed_data.size();
		return results;
	}
}
