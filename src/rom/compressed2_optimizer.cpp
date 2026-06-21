#include "rom/compressed2_optimizer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <utility>

namespace spintool::rom
{
	namespace
	{
		constexpr Uint16 kClearCode = 0x0100U;
		constexpr Uint16 kEndCode = 0x0101U;
		constexpr Uint16 kFirstDictionaryCode = 0x0102U;
		constexpr Uint16 kMaximumDictionaryCode = 0x07FFU;

		class LsbBitWriter
		{
		public:
			void Write(const Uint16 value, const unsigned int bit_count)
			{
				m_bit_buffer |= static_cast<std::uint64_t>(value) << m_buffered_bits;
				m_buffered_bits += bit_count;
				m_total_bits += bit_count;
				while (m_buffered_bits >= 8U)
				{
					m_bytes.emplace_back(static_cast<Uint8>(m_bit_buffer & 0xFFU));
					m_bit_buffer >>= 8U;
					m_buffered_bits -= 8U;
				}
			}

			std::vector<Uint8> Finish()
			{
				if (m_buffered_bits != 0U)
				{
					m_bytes.emplace_back(static_cast<Uint8>(m_bit_buffer & 0xFFU));
				}
				return std::move(m_bytes);
			}

			std::size_t GetTotalBits() const
			{
				return m_total_bits;
			}

		private:
			std::vector<Uint8> m_bytes;
			std::uint64_t m_bit_buffer = 0U;
			unsigned int m_buffered_bits = 0U;
			std::size_t m_total_bits = 0U;
		};

		struct EncodingPolicy
		{
			Uint16 dictionary_limit = kMaximumDictionaryCode;
			std::size_t maximum_segment_bytes = 0U;
			std::vector<std::size_t> forced_reset_positions;
			bool adaptive = false;
			std::size_t adaptive_minimum_bytes = 0U;
			double adaptive_degradation = 0.0;
			std::string name;
		};

		struct EncodedCandidate
		{
			std::vector<Uint8> data;
			std::size_t bit_count = 0U;
			std::string strategy;
		};

		EncodedCandidate EncodeWithPolicy(
			const std::vector<Uint8>& source,
			const EncodingPolicy& policy
		)
		{
			const Uint8 fallback_zero = 0U;
			const Uint8* input = source.empty() ? &fallback_zero : source.data();
			const std::size_t input_size = source.empty() ? 1U : source.size();

			LsbBitWriter bit_writer;
			std::unordered_map<Uint32, Uint16> transitions;
			transitions.reserve(2048U);

			Uint16 next_dictionary_code = kFirstDictionaryCode;
			unsigned int stream_width = 9U;
			Uint16 decoder_next_code = kFirstDictionaryCode;
			Uint16 decoder_width_threshold = 0x0200U;
			bool first_data_code_after_clear = true;
			std::size_t segment_start_position = 0U;
			std::size_t segment_start_bits = 0U;
			double best_adaptive_ratio = std::numeric_limits<double>::infinity();
			std::size_t next_forced_reset_index = 0U;

			auto reset_encoder_dictionary = [&]()
			{
				transitions.clear();
				next_dictionary_code = kFirstDictionaryCode;
			};
			auto reset_stream_state = [&]()
			{
				stream_width = 9U;
				decoder_next_code = kFirstDictionaryCode;
				decoder_width_threshold = 0x0200U;
				first_data_code_after_clear = true;
			};
			auto write_clear = [&](const std::size_t next_segment_position)
			{
				// CLEAR itself uses the width active before the reset.
				bit_writer.Write(kClearCode, stream_width);
				reset_encoder_dictionary();
				reset_stream_state();
				segment_start_position = next_segment_position;
				segment_start_bits = bit_writer.GetTotalBits();
				best_adaptive_ratio = std::numeric_limits<double>::infinity();
			};
			auto write_data_code = [&](const Uint16 code)
			{
				bit_writer.Write(code, stream_width);
				if (first_data_code_after_clear)
				{
					first_data_code_after_clear = false;
					return;
				}
				++decoder_next_code;
				if (decoder_next_code >= decoder_width_threshold && stream_width < 11U)
				{
					++stream_width;
					decoder_width_threshold = static_cast<Uint16>(decoder_width_threshold << 1U);
				}
			};

			write_clear(0U);
			Uint16 current_code = input[0];
			for (std::size_t position = 1U; position < input_size; ++position)
			{
				while (next_forced_reset_index < policy.forced_reset_positions.size() &&
					policy.forced_reset_positions[next_forced_reset_index] < position)
				{
					++next_forced_reset_index;
				}
				const bool forced_reset =
					next_forced_reset_index < policy.forced_reset_positions.size() &&
					policy.forced_reset_positions[next_forced_reset_index] == position;
				const bool fixed_segment_reset = policy.maximum_segment_bytes != 0U &&
					position - segment_start_position >= policy.maximum_segment_bytes;
				if (forced_reset || fixed_segment_reset)
				{
					write_data_code(current_code);
					write_clear(position);
					current_code = input[position];
					if (forced_reset)
					{
						++next_forced_reset_index;
					}
					continue;
				}

				const Uint8 next_byte = input[position];
				const Uint32 transition_key =
					(static_cast<Uint32>(current_code) << 8U) |
					static_cast<Uint32>(next_byte);
				const auto found = transitions.find(transition_key);
				if (found != transitions.end())
				{
					current_code = found->second;
					continue;
				}

				write_data_code(current_code);

				bool adaptive_reset = false;
				if (policy.adaptive)
				{
					const std::size_t segment_bytes = position - segment_start_position;
					if (segment_bytes >= policy.adaptive_minimum_bytes)
					{
						const std::size_t segment_bits =
							bit_writer.GetTotalBits() - segment_start_bits;
						const double ratio = static_cast<double>(segment_bits) /
							static_cast<double>(segment_bytes);
						if (!std::isfinite(best_adaptive_ratio) || ratio < best_adaptive_ratio)
						{
							best_adaptive_ratio = ratio;
						}
						else if (next_dictionary_code >= 0x0300U &&
							ratio > best_adaptive_ratio * (1.0 + policy.adaptive_degradation))
						{
							adaptive_reset = true;
						}
					}
				}

				if (adaptive_reset || next_dictionary_code > policy.dictionary_limit)
				{
					write_clear(position);
				}
				else if (next_dictionary_code <= kMaximumDictionaryCode)
				{
					transitions.emplace(transition_key, next_dictionary_code);
					++next_dictionary_code;
				}
				else
				{
					write_clear(position);
				}
				current_code = next_byte;
			}

			write_data_code(current_code);
			bit_writer.Write(kEndCode, stream_width);
			EncodedCandidate result;
			result.bit_count = bit_writer.GetTotalBits();
			result.data = bit_writer.Finish();
			result.strategy = policy.name;
			return result;
		}

		bool IsBetter(const EncodedCandidate& candidate, const EncodedCandidate& current)
		{
			if (current.data.empty())
			{
				return true;
			}
			if (candidate.data.size() != current.data.size())
			{
				return candidate.data.size() < current.data.size();
			}
			return candidate.bit_count < current.bit_count;
		}

		std::vector<std::size_t> ShiftResetPositions(
			const std::vector<std::size_t>& positions,
			const std::ptrdiff_t shift,
			const std::size_t source_size
		)
		{
			std::vector<std::size_t> shifted;
			shifted.reserve(positions.size());
			for (const std::size_t position : positions)
			{
				const std::ptrdiff_t value = static_cast<std::ptrdiff_t>(position) + shift;
				if (value <= 0 || static_cast<std::size_t>(value) >= source_size)
				{
					continue;
				}
				const std::size_t converted = static_cast<std::size_t>(value);
				if (shifted.empty() || shifted.back() != converted)
				{
					shifted.emplace_back(converted);
				}
			}
			return shifted;
		}
	}

	bool Compressed2Optimizer::Decode(
		const std::vector<Uint8>& input,
		const std::size_t offset,
		std::vector<Uint8>& output,
		std::string& error,
		std::size_t* consumed_size,
		std::vector<std::size_t>* reset_output_positions
	)
	{
		output.clear();
		error.clear();
		if (consumed_size)
		{
			*consumed_size = 0U;
		}
		if (reset_output_positions)
		{
			reset_output_positions->clear();
		}
		if (offset >= input.size())
		{
			error = "Compressed2 start offset is outside the input";
			return false;
		}

		std::array<std::vector<Uint8>, kMaximumDictionaryCode + 1U> dictionary;
		auto reset_dictionary = [&dictionary]()
		{
			for (std::vector<Uint8>& entry : dictionary)
			{
				entry.clear();
			}
			for (Uint16 value = 0U; value < 0x0100U; ++value)
			{
				dictionary[value].assign(1U, static_cast<Uint8>(value));
			}
		};

		std::size_t bit_position = 0U;
		unsigned int width = 9U;
		Uint16 next_code = kFirstDictionaryCode;
		Uint16 next_width_threshold = 0x0200U;
		std::vector<Uint8> previous;
		bool have_previous = false;
		reset_dictionary();

		auto read_code = [&](Uint16& code) -> bool
		{
			const std::size_t available_bits = (input.size() - offset) * 8U;
			if (bit_position > available_bits || width > available_bits - bit_position)
			{
				return false;
			}
			Uint32 value = 0U;
			for (unsigned int bit = 0U; bit < width; ++bit)
			{
				const std::size_t absolute_bit = bit_position + bit;
				const Uint8 byte = input[offset + absolute_bit / 8U];
				value |= static_cast<Uint32>((byte >> (absolute_bit & 7U)) & 1U) << bit;
			}
			bit_position += width;
			code = static_cast<Uint16>(value);
			return true;
		};

		for (std::size_t token_count = 0U; token_count < 1000000U; ++token_count)
		{
			Uint16 code = 0U;
			if (!read_code(code))
			{
				error = "Unexpected end of Compressed2 stream";
				return false;
			}
			if (code == kEndCode)
			{
				if (consumed_size)
				{
					*consumed_size = (bit_position + 7U) / 8U;
				}
				return true;
			}
			if (code == kClearCode)
			{
				if (reset_output_positions && !output.empty())
				{
					reset_output_positions->emplace_back(output.size());
				}
				reset_dictionary();
				width = 9U;
				next_code = kFirstDictionaryCode;
				next_width_threshold = 0x0200U;
				have_previous = false;

				if (!read_code(code))
				{
					error = "Compressed2 CLEAR token has no following literal";
					return false;
				}
				if (code == kEndCode)
				{
					if (consumed_size)
					{
						*consumed_size = (bit_position + 7U) / 8U;
					}
					return true;
				}
				if (code > 0x00FFU)
				{
					error = "Compressed2 CLEAR token is not followed by a literal";
					return false;
				}
				previous.assign(1U, static_cast<Uint8>(code));
				output.emplace_back(static_cast<Uint8>(code));
				have_previous = true;
				continue;
			}

			std::vector<Uint8> current;
			if (code <= kMaximumDictionaryCode && !dictionary[code].empty())
			{
				current = dictionary[code];
			}
			else if (have_previous && code == next_code)
			{
				current = previous;
				current.emplace_back(previous.front());
			}
			else
			{
				error = "Invalid Compressed2 dictionary code";
				return false;
			}

			output.insert(output.end(), current.begin(), current.end());
			if (have_previous && next_code <= kMaximumDictionaryCode)
			{
				dictionary[next_code] = previous;
				dictionary[next_code].emplace_back(current.front());
			}
			++next_code;
			if (next_code >= next_width_threshold && width < 11U)
			{
				++width;
				next_width_threshold = static_cast<Uint16>(next_width_threshold << 1U);
			}
			previous = std::move(current);
			have_previous = true;
		}

		error = "Compressed2 validation exceeded the token limit";
		return false;
	}

	Compressed2CompressionResult Compressed2Optimizer::Compress(
		const std::vector<Uint8>& source,
		const std::vector<Uint8>& original_stream
	)
	{
		Compressed2CompressionResult result;
		EncodedCandidate best;

		auto test_policy = [&](EncodingPolicy policy)
		{
			EncodedCandidate candidate = EncodeWithPolicy(source, policy);
			++result.candidates_tested;
			if (IsBetter(candidate, best))
			{
				best = std::move(candidate);
			}
		};

		EncodingPolicy baseline;
		baseline.name = "full dictionary";
		test_policy(baseline);
		result.baseline_size = best.data.size();

		std::vector<std::size_t> original_resets;
		if (!original_stream.empty())
		{
			std::vector<Uint8> original_output;
			std::string decode_error;
			std::size_t consumed = 0U;
			if (Decode(
				original_stream,
				0U,
				original_output,
				decode_error,
				&consumed,
				&original_resets
			))
			{
				if (original_output == source && consumed <= original_stream.size())
				{
					EncodedCandidate original_candidate;
					original_candidate.data.assign(
						original_stream.begin(),
						original_stream.begin() + static_cast<std::ptrdiff_t>(consumed)
					);
					original_candidate.bit_count = consumed * 8U;
					original_candidate.strategy = "original stream preserved";
					++result.candidates_tested;
					if (original_candidate.data.size() <= best.data.size())
					{
						best = std::move(original_candidate);
					}
				}
				if (!original_resets.empty())
				{
					EncodingPolicy original_layout;
					original_layout.forced_reset_positions = original_resets;
					original_layout.name = "original reset layout";
					test_policy(std::move(original_layout));

					constexpr std::array<std::ptrdiff_t, 8> shifts =
					{
						-256, -128, -64, -32, 32, 64, 128, 256
					};
					for (const std::ptrdiff_t shift : shifts)
					{
						EncodingPolicy shifted_layout;
						shifted_layout.forced_reset_positions = ShiftResetPositions(
							original_resets,
							shift,
							source.size()
						);
						if (shifted_layout.forced_reset_positions.empty())
						{
							continue;
						}
						shifted_layout.name = "shifted original reset layout";
						test_policy(std::move(shifted_layout));
					}
				}
			}
		}

		// Test smaller dictionary limits. Resetting before the table reaches its
		// maximum can save enough 10/11-bit codes to offset the extra CLEAR token.
		for (Uint16 limit = 0x0140U; limit < kMaximumDictionaryCode; limit =
			static_cast<Uint16>(limit + 0x0020U))
		{
			EncodingPolicy policy;
			policy.dictionary_limit = limit;
			policy.name = "early dictionary reset";
			test_policy(std::move(policy));
		}
		for (const Uint16 limit : { Uint16(0x01FFU), Uint16(0x03FFU), Uint16(0x05FFU) })
		{
			EncodingPolicy policy;
			policy.dictionary_limit = limit;
			policy.name = "code-width boundary reset";
			test_policy(std::move(policy));
		}

		// Also test fixed source segment sizes. This is effective when an art block
		// contains several unrelated sprite groups with very different patterns.
		constexpr std::array<std::size_t, 27> segment_sizes =
		{
			128U, 160U, 192U, 224U, 256U, 320U, 384U, 448U, 512U,
			640U, 768U, 896U, 1024U, 1280U, 1536U, 1792U, 2048U,
			2560U, 3072U, 3584U, 4096U, 5120U, 6144U, 7168U,
			8192U, 12288U, 16384U
		};
		for (const std::size_t segment_size : segment_sizes)
		{
			if (segment_size >= source.size())
			{
				continue;
			}
			EncodingPolicy policy;
			policy.maximum_segment_bytes = segment_size;
			policy.name = "fixed-size segments";
			test_policy(std::move(policy));
		}

		// Adaptive policies clear the dictionary only after its measured coding
		// efficiency has degraded. Several thresholds are tried; the smallest valid
		// result wins, so no single heuristic is trusted unconditionally.
		for (const std::size_t minimum_bytes : { 384U, 512U, 768U, 1024U, 1536U })
		{
			for (const double degradation : { 0.01, 0.02, 0.04, 0.08 })
			{
				if (minimum_bytes >= source.size())
				{
					continue;
				}
				EncodingPolicy policy;
				policy.adaptive = true;
				policy.adaptive_minimum_bytes = minimum_bytes;
				policy.adaptive_degradation = degradation;
				policy.name = "adaptive ratio reset";
				test_policy(std::move(policy));
			}
		}

		// A final independent decode guards every caller against an encoder bug.
		std::vector<Uint8> verified;
		std::string validation_error;
		std::size_t consumed = 0U;
		if (!Decode(best.data, 0U, verified, validation_error, &consumed, nullptr) ||
			verified != (source.empty() ? std::vector<Uint8>{ 0U } : source) ||
			consumed != best.data.size())
		{
			// The baseline is built by the same compatible encoder but keep a safe,
			// deterministic fallback rather than returning a possibly invalid stream.
			best = EncodeWithPolicy(source, baseline);
			best.strategy = "validated full dictionary fallback";
		}

		result.data = std::move(best.data);
		result.strategy = std::move(best.strategy);
		return result;
	}
}
