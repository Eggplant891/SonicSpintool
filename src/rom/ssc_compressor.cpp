#include "rom/ssc_compressor.h"
#include <cassert>

namespace spintool::rom
{
	SSCCompressionResult SSCCompressor::CompressData(const std::vector<Uint8>& in_data, Uint32 offset, Uint32 working_data_size_hint)
	{
		// Initial implementation will just push uncompressed data entirely

		std::vector<Uint8> out_data;

		size_t current_data_index = 0u;

		while (current_data_index < in_data.size())
		{
			const size_t size_diff = std::min(in_data.size() - current_data_index, 8ull);
			Uint8 fragment_header = 0x00;
			switch (size_diff)
			{
				case 8: fragment_header |= 0x80;
				case 7: fragment_header |= 0x40;
				case 6: fragment_header |= 0x20;
				case 5: fragment_header |= 0x10;
				case 4: fragment_header |= 0x08;
				case 3: fragment_header |= 0x04;
				case 2: fragment_header |= 0x02;
				case 1: fragment_header |= 0x01;
			}
			out_data.emplace_back(fragment_header);

			for (size_t fragments_remaining = size_diff; fragments_remaining > 0; --fragments_remaining)
			{
				const size_t read_index = (current_data_index + size_diff) - fragments_remaining;
				assert(read_index < in_data.size());
				out_data.emplace_back(in_data.at(read_index));
			}

			if (current_data_index + size_diff == in_data.size())
			{
				out_data.emplace_back(0u);
				out_data.emplace_back(0u);
				out_data.emplace_back(0u);
				break;
			}
			else
			{
				current_data_index += 8u;
			}
		}

		return out_data;
	}

}