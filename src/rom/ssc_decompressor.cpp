#include "rom/ssc_decompressor.h"

namespace spintool::rom
{
    SSCDecompressionResult SSCDecompressor::IsValidSSCCompressedData(const Uint8* in_data, const Uint32 starting_offset)
    {
        SSCDecompressionResult results;
        if (!in_data)
        {
            results.error_msg = "Null SSC input pointer";
            return results;
        }

        // This legacy pointer-only validator cannot know the end of the ROM.
        // Keep it for compatibility, but bounded loading uses DecompressData().
        const Uint8* current_byte = in_data;
        if ((*current_byte & 1) == 0)
        {
            results.rom_data.SetROMData(starting_offset, starting_offset + 1);
            results.error_msg = "SSC stream does not begin with a raw fragment";
            return results;
        }

        results.rom_data.SetROMData(starting_offset, starting_offset + 1);
        results.uncompressed_size = 1;
        return results;
    }

    SSCDecompressionResult SSCDecompressor::DecompressData(
        const std::vector<Uint8>& in_data,
        const Uint32 offset,
        const Uint32 working_data_size_hint)
    {
        SSCDecompressionResult results;
        if (offset >= in_data.size())
        {
            results.error_msg = "SSC offset is outside the input buffer";
            return results;
        }

        std::vector<Uint8> working_data(0x1000, 0);
        auto& out_data = results.uncompressed_data;
        out_data.reserve(std::min<size_t>(working_data_size_hint, 16U * 1024U * 1024U));

        size_t current = offset;
        constexpr size_t max_output_size = 16U * 1024U * 1024U;
        constexpr size_t max_fragments = 32U * 1024U * 1024U;
        size_t processed_fragments = 0;
        bool end_reached = false;

        while (!end_reached)
        {
            if (current >= in_data.size())
            {
                results.error_msg = "Unexpected end of SSC stream while reading a fragment header";
                break;
            }

            Uint8 fragment_header = in_data[current++];
            for (int fragments_remaining = 0; fragments_remaining < 8; ++fragments_remaining)
            {
                if (++processed_fragments > max_fragments)
                {
                    results.error_msg = "SSC stream did not terminate safely";
                    end_reached = true;
                    break;
                }

                const bool is_raw_data = (fragment_header & 1U) != 0;
                fragment_header >>= 1U;

                if (is_raw_data)
                {
                    if (current >= in_data.size())
                    {
                        results.error_msg = "Unexpected end of SSC stream while reading raw data";
                        end_reached = true;
                        break;
                    }
                    const Uint8 value = in_data[current++];
                    out_data.emplace_back(value);
                    working_data[out_data.size() & 0xFFFU] = value;
                }
                else
                {
                    if (current > in_data.size() || in_data.size() - current < 2)
                    {
                        results.error_msg = "Unexpected end of SSC stream while reading a copy token";
                        end_reached = true;
                        break;
                    }

                    Uint16 source = static_cast<Uint16>(in_data[current]);
                    const Uint8 control = in_data[current + 1];
                    current += 2;
                    source |= static_cast<Uint16>(control & 0xF0U) << 4U;

                    if (source == 0)
                    {
                        end_reached = true;
                        break;
                    }

                    const size_t copy_count = static_cast<size_t>(control & 0x0FU) + 2U;
                    if (out_data.size() > max_output_size || copy_count > max_output_size - out_data.size())
                    {
                        results.error_msg = "SSC output exceeded the safety limit";
                        end_reached = true;
                        break;
                    }

                    for (size_t i = 0; i < copy_count; ++i)
                    {
                        const Uint8 value = working_data[source & 0xFFFU];
                        out_data.emplace_back(value);
                        working_data[out_data.size() & 0xFFFU] = value;
                        source = static_cast<Uint16>((source + 1U) & 0xFFFU);
                    }
                }

                if (out_data.size() > max_output_size)
                {
                    results.error_msg = "SSC output exceeded the safety limit";
                    end_reached = true;
                    break;
                }
            }
        }

        results.rom_data.SetROMData(offset, static_cast<Uint32>(std::min(current, in_data.size())));
        results.uncompressed_size = out_data.size();
        return results;
    }
}
