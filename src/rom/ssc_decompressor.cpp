#include "rom/ssc_decompressor.h"

namespace spintool::rom
{
	std::vector<Uint8> SSCDecompressor::DecompressData(const Uint8* in_data, size_t size_hint)
	{
		const Uint8* compressed_data_root = in_data;

		std::vector<Uint8> out_data;
		std::vector<Uint8> working_data;

		out_data.reserve(size_hint); // Size needs to be accurate to determine where to index in working_data.
		working_data.resize(size_hint); // Needs to be indexable even before we have written to it

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
						const Uint8 value_to_write = working_data.at(tile_index_offset);
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
		return out_data;
	}
}
