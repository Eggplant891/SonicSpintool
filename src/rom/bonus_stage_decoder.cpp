#include "rom/bonus_stage_decoder.h"

#include "rom/tile.h"
#include "rom/spinball_rom.h"
#include "rom/sprite.h"
#include "rom/tileset.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace spintool::rom
{
	namespace
	{
		constexpr Uint32 kCommonBackgroundArt = 0x000C77B0;
		constexpr Uint32 kCommonForegroundArt = 0x000C9016;
		constexpr Uint32 kCommonBackgroundVram = 0x0000;
		constexpr Uint32 kCommonForegroundVram = 0x24A0;
		constexpr Uint32 kStageArtVram = 0x56A0;
		constexpr Uint16 kCommonForegroundTile = static_cast<Uint16>(
			kCommonForegroundVram / TileSet::s_tile_total_bytes
		); // 0x0125, stored in RAM word $FF5486.
		constexpr Uint16 kStageArtTile = static_cast<Uint16>(
			kStageArtVram / TileSet::s_tile_total_bytes
		); // 0x02B5, stored in RAM word $FF5482.
		constexpr std::size_t kVramSize = 0x10000;

		constexpr Uint16 kTileIndexMask = 0x07FF;
		constexpr Uint16 kHorizontalFlip = 0x0800;
		constexpr Uint16 kVerticalFlip = 0x1000;
		constexpr Uint16 kPaletteMask = 0x6000;
		constexpr Uint16 kPriorityMask = 0x8000;

		struct AnimationStateDefinition
		{
			const char* name;
			Uint32 animation_root;
		};

		struct ObjectDefinition
		{
			const char* name;
			Uint32 object_ram_address;
			Uint32 initial_mapping;
			Uint16 base_tile_attributes;
			const AnimationStateDefinition* states;
			std::size_t state_count;
			bool is_common_object;
		};

		struct StageDefinition
		{
			const char* name;
			Uint32 art_offset;
			Uint32 mapping_region_start;
			Uint32 mapping_region_end;
			const ObjectDefinition* objects;
			std::size_t object_count;
		};

		constexpr Uint16 StageBase(Uint16 flags)
		{
			return static_cast<Uint16>(kStageArtTile + flags);
		}

		constexpr Uint16 CommonBase(Uint16 flags)
		{
			return static_cast<Uint16>(kCommonForegroundTile + flags);
		}

		template <std::size_t N>
		constexpr std::size_t CountStates(const AnimationStateDefinition (&)[N])
		{
			return N;
		}

		constexpr AnimationStateDefinition kRoboOuterStates[] =
		{
			{ "Initial animation", 0x000D2296 },
			{ "Runtime state A",  0x000D22A0 },
			{ "Runtime state B",  0x000D2304 },
		};
		constexpr AnimationStateDefinition kRoboInnerStates[] =
		{
			{ "Initial animation", 0x000D219C },
			{ "Runtime state A",  0x000D21A6 },
			{ "Runtime state B",  0x000D220A },
		};
		constexpr AnimationStateDefinition kRoboCenterStates[] =
		{
			{ "Initial / intact face",             0x000D237C },
			{ "Final explosion / face destroyed",  0x000D2390 },
		};
		constexpr AnimationStateDefinition kRoboTargetStates[] =
		{
			{ "Initial tooth",                    0x000D212E },
			{ "Tooth hit / activated",            0x000D2142 },
			{ "All teeth destroyed / final state", 0x000D217E },
		};
		constexpr AnimationStateDefinition kRoboTargetAltStates[] =
		{
			{ "Initial animation", 0x000D2138 },
		};

		constexpr AnimationStateDefinition kCluckerCrabStates[] =
		{
			{ "Initial animation", 0x000D2F8E },
			{ "Runtime state A",  0x000D2FB6 },
			{ "Runtime state B",  0x000D2FC0 },
			{ "Runtime state C",  0x000D2FCA },
			{ "Runtime state D",  0x000D2FD4 },
			{ "Final state",      0x000D2FDE },
		};
		constexpr AnimationStateDefinition kCluckerBirdStates[] =
		{
			{ "Initial animation", 0x000D310A },
		};

		// TRAPPED ALIVE uses two linked objects for Robotnik and the Eggomatic.
		// Their animations are changed by the end-of-stage state machine after
		// all seven animals have been freed. Keep every runtime root separately:
		// several roots share mappings but represent distinct editable states.
		constexpr AnimationStateDefinition kTrappedRobotnikStates[] =
		{
			{ "Initial / idle",                  0x000D2ADE },
			{ "Move left",                      0x000D2B4C },
			{ "Move right",                     0x000D2B2E },
			{ "Turn / recover from left",       0x000D2AF2 },
			{ "Turn / recover from right",      0x000D2B10 },
			{ "Ship explosion trigger",         0x000D2C32 },
			{ "Explosion / Robotnik ejected",   0x000D2BEC },
		};
		constexpr AnimationStateDefinition kTrappedEggomaticStates[] =
		{
			{ "Initial / idle",             0x000D2B6A },
			{ "Move left",                 0x000D2BCE },
			{ "Move right",                0x000D2BB0 },
			{ "Turn / recover from left",  0x000D2B74 },
			{ "Turn / recover from right", 0x000D2B92 },
			{ "Destroyed variant A",       0x000D2CA0 },
			{ "Destroyed variant B",       0x000D2D4A },
		};

		#define TRAPPED_ANIMAL_STATES(name, final_root) \
			constexpr AnimationStateDefinition name[] = \
			{ \
				{ "Initial animation", 0x000D28E0 }, \
				{ "Hit state 1",      0x000D28EA }, \
				{ "Hit state 2",      0x000D299E }, \
				{ "Freed animation",  final_root }, \
			}
		TRAPPED_ANIMAL_STATES(kTrappedAnimal1States, 0x000D2E30);
		TRAPPED_ANIMAL_STATES(kTrappedAnimal2States, 0x000D2E26);
		TRAPPED_ANIMAL_STATES(kTrappedAnimal3States, 0x000D2DFE);
		TRAPPED_ANIMAL_STATES(kTrappedAnimal4States, 0x000D2E08);
		TRAPPED_ANIMAL_STATES(kTrappedAnimal5States, 0x000D2E12);
		TRAPPED_ANIMAL_STATES(kTrappedAnimal6States, 0x000D2E1C);
		TRAPPED_ANIMAL_STATES(kTrappedAnimal7States, 0x000D2DF4);
		#undef TRAPPED_ANIMAL_STATES

		constexpr AnimationStateDefinition kMarchObject1States[] =
		{
			{ "Initial animation", 0x000D2638 },
		};
		constexpr AnimationStateDefinition kMarchObject2States[] =
		{
			{ "Initial animation", 0x000D28D6 },
			{ "Runtime state A",  0x000D28B8 },
			{ "Runtime state B",  0x000D280E },
			{ "Completion",       0x000D2642 },
		};
		constexpr AnimationStateDefinition kMarchObject3States[] =
		{
			{ "Initial animation", 0x000D28AE },
			{ "Runtime state A",  0x000D2890 },
			{ "Runtime state B",  0x000D2804 },
		};
		constexpr AnimationStateDefinition kMarchObject4States[] =
		{
			{ "Initial animation", 0x000D2886 },
			{ "Runtime state A",  0x000D2868 },
			{ "Runtime state B",  0x000D27FA },
		};
		constexpr AnimationStateDefinition kMarchObject5States[] =
		{
			{ "Initial animation", 0x000D285E },
			{ "Runtime state A",  0x000D2840 },
			{ "Runtime state B",  0x000D27F0 },
		};
		constexpr AnimationStateDefinition kMarchObject6States[] =
		{
			{ "Initial animation", 0x000D2836 },
			{ "Runtime state A",  0x000D2818 },
			{ "Runtime state B",  0x000D27E6 },
		};
		constexpr AnimationStateDefinition kMarchObject7States[] =
		{
			{ "Initial animation", 0x000D27DC },
		};
		#define SINGLE_MARCH_STATE(name, root) \
			constexpr AnimationStateDefinition name[] = { { "Initial animation", root } }
		SINGLE_MARCH_STATE(kMarchObject8States,  0x000D2598);
		SINGLE_MARCH_STATE(kMarchObject9States,  0x000D25C0);
		SINGLE_MARCH_STATE(kMarchObject10States, 0x000D25E8);
		SINGLE_MARCH_STATE(kMarchObject11States, 0x000D2610);
		SINGLE_MARCH_STATE(kMarchObject12States, 0x000D25AC);
		SINGLE_MARCH_STATE(kMarchObject13States, 0x000D25D4);
		SINGLE_MARCH_STATE(kMarchObject14States, 0x000D25FC);
		SINGLE_MARCH_STATE(kMarchObject15States, 0x000D2624);
		#undef SINGLE_MARCH_STATE

		// Common static objects created for every Bonus Stage. Every duplicate
		// instance is retained because its base attributes and RAM identity matter.
		constexpr ObjectDefinition kCommonObjects[] =
		{
			{ "Common static C8D8A #1", 0x00FF3F3C, 0x000C8D8A, CommonBase(0x4000), nullptr, 0, true },
			{ "Common static C8DA0 #1", 0x00FF3F5C, 0x000C8DA0, CommonBase(0x4000), nullptr, 0, true },
			{ "Common static C8D8A #2", 0x00FF3F7C, 0x000C8D8A, CommonBase(0x4800), nullptr, 0, true },
			{ "Common static C8DA0 #2", 0x00FF3F9C, 0x000C8DA0, CommonBase(0x4800), nullptr, 0, true },
			{ "Common static C8DBE #1", 0x00FF3FBC, 0x000C8DBE, CommonBase(0xC000), nullptr, 0, true },
			{ "Common static C8DBE #2", 0x00FF3FDC, 0x000C8DBE, CommonBase(0xC800), nullptr, 0, true },
			{ "Common static C8E6A #1", 0x00FF40C0, 0x000C8E6A, CommonBase(0x2000), nullptr, 0, true },
			{ "Common static C8E78 #1", 0x00FF4100, 0x000C8E78, CommonBase(0xC000), nullptr, 0, true },
			{ "Common static C8EC2 #1", 0x00FF4120, 0x000C8EC2, CommonBase(0x2000), nullptr, 0, true },
			{ "Common static C8E6A #2", 0x00FF4140, 0x000C8E6A, CommonBase(0x2800), nullptr, 0, true },
			{ "Common static C8E78 #2", 0x00FF4180, 0x000C8E78, CommonBase(0xC800), nullptr, 0, true },
			{ "Common static C8EC2 #2", 0x00FF41A0, 0x000C8EC2, CommonBase(0x2800), nullptr, 0, true },
		};

		constexpr ObjectDefinition kRoboObjects[] =
		{
			{ "Outer object #1", 0x00FF75B4, 0x000CB062, StageBase(0x6000), kRoboOuterStates, CountStates(kRoboOuterStates), false },
			{ "Outer object #2 mirrored", 0x00FF75D4, 0x000CB062, StageBase(0x6800), kRoboOuterStates, CountStates(kRoboOuterStates), false },
			{ "Inner object #1", 0x00FF75F4, 0x000CB01C, StageBase(0x6000), kRoboInnerStates, CountStates(kRoboInnerStates), false },
			{ "Inner object #2 mirrored", 0x00FF7614, 0x000CB01C, StageBase(0x6800), kRoboInnerStates, CountStates(kRoboInnerStates), false },
			{ "Center object", 0x00FF7634, 0x000CA362, StageBase(0x6000), kRoboCenterStates, CountStates(kRoboCenterStates), false },
			{ "Target #1", 0x00FF7654, 0x000CB0EC, StageBase(0x0000), kRoboTargetStates, CountStates(kRoboTargetStates), false },
			{ "Target #2", 0x00FF7674, 0x000CB108, StageBase(0x0000), kRoboTargetAltStates, CountStates(kRoboTargetAltStates), false },
			{ "Target #3", 0x00FF7694, 0x000CB0EC, StageBase(0x0000), kRoboTargetStates, CountStates(kRoboTargetStates), false },
			{ "Target #4", 0x00FF76B4, 0x000CB108, StageBase(0x0000), kRoboTargetAltStates, CountStates(kRoboTargetAltStates), false },
			{ "Target #5", 0x00FF76D4, 0x000CB0EC, StageBase(0x0000), kRoboTargetStates, CountStates(kRoboTargetStates), false },
			{ "Target #6", 0x00FF76F4, 0x000CB108, StageBase(0x0000), kRoboTargetAltStates, CountStates(kRoboTargetAltStates), false },
		};

		constexpr ObjectDefinition kCluckerObjects[] =
		{
			{ "Main target", 0x00FF75B4, 0x000D0EC0, StageBase(0x6000), kCluckerCrabStates, CountStates(kCluckerCrabStates), false },
			{ "Clucker #1", 0x00FF75D4, 0x000D0E36, StageBase(0x6000), kCluckerBirdStates, CountStates(kCluckerBirdStates), false },
			{ "Clucker #2", 0x00FF75F4, 0x000D0E36, StageBase(0x6000), kCluckerBirdStates, CountStates(kCluckerBirdStates), false },
		};

		constexpr ObjectDefinition kTrappedObjects[] =
		{
			{ "Animal #1", 0x00FF75B4, 0x000CC6FA, StageBase(0x6000), kTrappedAnimal1States, CountStates(kTrappedAnimal1States), false },
			{ "Animal #2", 0x00FF75D4, 0x000CC6FA, StageBase(0x6000), kTrappedAnimal2States, CountStates(kTrappedAnimal2States), false },
			{ "Animal #3", 0x00FF75F4, 0x000CC6FA, StageBase(0x6000), kTrappedAnimal3States, CountStates(kTrappedAnimal3States), false },
			{ "Animal #4", 0x00FF7614, 0x000CC6FA, StageBase(0x6000), kTrappedAnimal4States, CountStates(kTrappedAnimal4States), false },
			{ "Animal #5", 0x00FF7634, 0x000CC6FA, StageBase(0x6000), kTrappedAnimal5States, CountStates(kTrappedAnimal5States), false },
			{ "Animal #6", 0x00FF7654, 0x000CC6FA, StageBase(0x6000), kTrappedAnimal6States, CountStates(kTrappedAnimal6States), false },
			{ "Animal #7", 0x00FF7674, 0x000CC6FA, StageBase(0x6000), kTrappedAnimal7States, CountStates(kTrappedAnimal7States), false },
			{ "Eggomatic / ship", 0x00FF7694, 0x000CC6FA, StageBase(0x6000), kTrappedEggomaticStates, CountStates(kTrappedEggomaticStates), false },
			{ "Dr. Robotnik",     0x00FF76B4, 0x000CC6FA, StageBase(0x6000), kTrappedRobotnikStates, CountStates(kTrappedRobotnikStates), false },
		};

		constexpr ObjectDefinition kMarchObjects[] =
		{
			{ "Chain object #1", 0x00FF75B4, 0x000CF04E, StageBase(0x6000), kMarchObject1States, CountStates(kMarchObject1States), false },
			{ "Chain object #2", 0x00FF75D4, 0x000CF05C, StageBase(0x6000), kMarchObject2States, CountStates(kMarchObject2States), false },
			{ "Chain object #3", 0x00FF75F4, 0x000CF078, StageBase(0x6000), kMarchObject3States, CountStates(kMarchObject3States), false },
			{ "Chain object #4", 0x00FF7614, 0x000CF094, StageBase(0x6000), kMarchObject4States, CountStates(kMarchObject4States), false },
			{ "Chain object #5", 0x00FF7634, 0x000CF0B0, StageBase(0x6000), kMarchObject5States, CountStates(kMarchObject5States), false },
			{ "Chain object #6", 0x00FF7654, 0x000CF0CC, StageBase(0x6000), kMarchObject6States, CountStates(kMarchObject6States), false },
			{ "Chain object #7", 0x00FF7774, 0x000CF04E, StageBase(0x6000), kMarchObject7States, CountStates(kMarchObject7States), false },
			{ "Marcher #1", 0x00FF7674, 0x000CF0E8, StageBase(0x6000), kMarchObject8States, CountStates(kMarchObject8States), false },
			{ "Marcher #2", 0x00FF7694, 0x000CF0E8, StageBase(0x6000), kMarchObject9States, CountStates(kMarchObject9States), false },
			{ "Marcher #3", 0x00FF76B4, 0x000CF0E8, StageBase(0x6000), kMarchObject10States, CountStates(kMarchObject10States), false },
			{ "Marcher #4", 0x00FF76D4, 0x000CF0E8, StageBase(0x6000), kMarchObject11States, CountStates(kMarchObject11States), false },
			{ "Marcher #5", 0x00FF76F4, 0x000CF0E8, StageBase(0x6000), kMarchObject12States, CountStates(kMarchObject12States), false },
			{ "Marcher #6", 0x00FF7714, 0x000CF0E8, StageBase(0x6000), kMarchObject13States, CountStates(kMarchObject13States), false },
			{ "Marcher #7", 0x00FF7734, 0x000CF0E8, StageBase(0x6000), kMarchObject14States, CountStates(kMarchObject14States), false },
			{ "Marcher #8", 0x00FF7754, 0x000CF0E8, StageBase(0x6000), kMarchObject15States, CountStates(kMarchObject15States), false },
		};

		constexpr StageDefinition kStages[] =
		{
			{ "ROBO SMILE", 0x000CB14E, 0x000CA362, 0x000CB14E, kRoboObjects, std::size(kRoboObjects) },
			{ "CLUCKER'S DEFENSE", 0x000D10B0, 0x000D0E36, 0x000D10B0, kCluckerObjects, std::size(kCluckerObjects) },
			{ "TRAPPED ALIVE", 0x000CC710, 0x000CC000, 0x000CC710, kTrappedObjects, std::size(kTrappedObjects) },
			{ "THE MARCH", 0x000CF2DE, 0x000CEF00, 0x000CF2DE, kMarchObjects, std::size(kMarchObjects) },
		};
		struct VirtualVram
		{
			std::array<Uint8, kVramSize> bytes{};
			std::array<bool, kVramSize> written{};
		};

		struct MappingPiece
		{
			Sint16 x = 0;
			Sint16 y = 0;
			Uint8 width_in_tiles = 0;
			Uint8 height_in_tiles = 0;
			Uint16 tile_attributes = 0;
		};

		struct ParsedMapping
		{
			Sint16 x_origin = 0;
			Sint16 y_origin = 0;
			Uint32 end_offset = 0;
			std::vector<MappingPiece> pieces;
		};

		// The object attributes provide the base tile index and may mirror the
		// complete mapping. Piece attributes provide an additional tile index and
		// may mirror an individual piece. Flip bits must therefore be combined
		// with XOR, not integer addition: 0x0800 + 0x0800 would incorrectly become
		// the vertical-flip bit 0x1000.
		Uint16 CombineTileAttributes(Uint16 object_attributes, Uint16 piece_attributes)
		{
			const Uint16 tile_index = static_cast<Uint16>(
				((object_attributes & kTileIndexMask) +
				 (piece_attributes & kTileIndexMask)) & kTileIndexMask
			);
			const Uint16 flips = static_cast<Uint16>(
				(object_attributes ^ piece_attributes) &
				(kHorizontalFlip | kVerticalFlip)
			);
			const Uint16 palette = static_cast<Uint16>(
				(object_attributes | piece_attributes) & kPaletteMask
			);
			const Uint16 priority = static_cast<Uint16>(
				(object_attributes | piece_attributes) & kPriorityMask
			);
			return static_cast<Uint16>(tile_index | flips | palette | priority);
		}

		int GetDisplayedPieceX(
			const MappingPiece& piece,
			const ParsedMapping& mapping,
			Uint16 object_attributes
		)
		{
			const int width = static_cast<int>(piece.width_in_tiles) * 8;
			const int relative_x = static_cast<int>(piece.x) -
				static_cast<int>(mapping.x_origin);
			return (object_attributes & kHorizontalFlip) != 0
				? -relative_x - width
				: relative_x;
		}

		int GetDisplayedPieceY(
			const MappingPiece& piece,
			const ParsedMapping& mapping,
			Uint16 object_attributes
		)
		{
			const int height = static_cast<int>(piece.height_in_tiles) * 8;
			const int relative_y = static_cast<int>(piece.y) -
				static_cast<int>(mapping.y_origin);
			return (object_attributes & kVerticalFlip) != 0
				? -relative_y - height
				: relative_y;
		}

		struct EditableArtBlock
		{
			Uint32 rom_offset = 0;
			Uint32 pointer_operand_offset = 0;
			Uint32 vram_offset = 0;
			std::size_t original_capacity = 0;
			std::size_t current_compressed_size = 0;
			std::vector<Uint8> data;
			bool touched = false;
		};

		struct BonusArtPointers
		{
			std::array<Uint32, 6> operand_offsets{};
			std::array<Uint32, 6> art_offsets{};
		};

		class LsbBitWriter
		{
		public:
			void Write(Uint16 value, unsigned int bit_count)
			{
				m_bit_buffer |= static_cast<std::uint64_t>(value) << m_buffered_bits;
				m_buffered_bits += bit_count;
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

		private:
			std::vector<Uint8> m_bytes;
			std::uint64_t m_bit_buffer = 0;
			unsigned int m_buffered_bits = 0;
		};

		// Exact LZW encoder for the Compressed2 stream consumed by
		// DoLoadCompressed2Tiles. The decoder uses a "late change" code-width
		// transition: the code which fills the current table is still read using
		// the old width, then the following code uses the new width. Keep a
		// separate decoder-side state so the emitted 9/10/11-bit widths match the
		// 68000 routine exactly.
		std::vector<Uint8> EncodeCompressed2LzwStream(
			const std::vector<Uint8>& source
		)
		{
			constexpr Uint16 clear_code = 0x0100;
			constexpr Uint16 end_code = 0x0101;
			constexpr Uint16 first_dictionary_code = 0x0102;
			constexpr Uint16 maximum_dictionary_code = 0x07FF;

			const Uint8 fallback_zero = 0;
			const Uint8* input = source.empty() ? &fallback_zero : source.data();
			const std::size_t input_size = source.empty() ? 1U : source.size();

			LsbBitWriter bit_writer;
			std::unordered_map<Uint32, Uint16> transitions;
			transitions.reserve(2048U);

			Uint16 next_dictionary_code = first_dictionary_code;
			unsigned int stream_width = 9U;
			Uint16 decoder_next_code = first_dictionary_code;
			Uint16 decoder_width_threshold = 0x0200;
			bool first_data_code_after_clear = true;

			auto reset_encoder_dictionary = [&]()
			{
				transitions.clear();
				next_dictionary_code = first_dictionary_code;
			};
			auto reset_stream_state = [&]()
			{
				stream_width = 9U;
				decoder_next_code = first_dictionary_code;
				decoder_width_threshold = 0x0200;
				first_data_code_after_clear = true;
			};
			auto write_clear = [&]()
			{
				// CLEAR itself is encoded using the width active before the reset.
				bit_writer.Write(clear_code, stream_width);
				reset_encoder_dictionary();
				reset_stream_state();
			};
			auto write_data_code = [&](Uint16 code)
			{
				bit_writer.Write(code, stream_width);
				if (first_data_code_after_clear)
				{
					// DoLoadCompressed2Tiles reads the first code after CLEAR through
					// a special literal path and does not add a dictionary entry.
					first_data_code_after_clear = false;
					return;
				}

				++decoder_next_code;
				if (decoder_next_code >= decoder_width_threshold && stream_width < 11U)
				{
					++stream_width;
					decoder_width_threshold = static_cast<Uint16>(
						decoder_width_threshold << 1U
					);
				}
			};

			write_clear();
			Uint16 current_code = input[0];
			for (std::size_t position = 1; position < input_size; ++position)
			{
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
				if (next_dictionary_code <= maximum_dictionary_code)
				{
					transitions.emplace(transition_key, next_dictionary_code);
					++next_dictionary_code;
				}
				else
				{
					// The 11-bit table is full. Reset before any code greater than
					// $7FF could be required by the decoder.
					write_clear();
				}
				current_code = next_byte;
			}

			write_data_code(current_code);
			bit_writer.Write(end_code, stream_width);
			return bit_writer.Finish();
		}

		bool ReadCompressed2Code(
			const std::vector<Uint8>& input,
			std::size_t& bit_position,
			unsigned int width,
			Uint16& code
		)
		{
			const std::size_t byte_position = bit_position / 8U;
			if (byte_position > input.size() || input.size() - byte_position < 3U)
			{
				return false;
			}
			const Uint32 window =
				static_cast<Uint32>(input[byte_position]) |
				(static_cast<Uint32>(input[byte_position + 1U]) << 8U) |
				(static_cast<Uint32>(input[byte_position + 2U]) << 16U);
			const Uint32 mask = (1U << width) - 1U;
			code = static_cast<Uint16>(
				(window >> (bit_position & 7U)) & mask
			);
			bit_position += width;
			return true;
		}

		// Independent validator for the exact token rules used by
		// DoLoadCompressed2Tiles. It does not call SpinTool's normal LZSS
		// decoder, so a matching result is not a circular self-test.
		bool DecodeCompressed2Reference(
			const std::vector<Uint8>& compressed,
			std::vector<Uint8>& output,
			std::string& error
		)
		{
			constexpr Uint16 clear_code = 0x0100;
			constexpr Uint16 end_code = 0x0101;
			constexpr Uint16 first_dictionary_code = 0x0102;
			constexpr Uint16 maximum_code = 0x07FF;

			std::array<std::vector<Uint8>, maximum_code + 1U> dictionary;
			auto reset_dictionary = [&dictionary]()
			{
				for (std::vector<Uint8>& entry : dictionary)
				{
					entry.clear();
				}
				for (Uint16 value = 0; value < 0x100; ++value)
				{
					dictionary[value].push_back(static_cast<Uint8>(value));
				}
			};

			output.clear();
			reset_dictionary();
			std::size_t bit_position = 0;
			unsigned int width = 9;
			Uint16 next_code = first_dictionary_code;
			Uint16 next_width_threshold = 0x0200;
			std::vector<Uint8> previous;
			bool have_previous = false;
			std::size_t token_count = 0;

			while (++token_count <= 1000000U)
			{
				Uint16 code = 0;
				if (!ReadCompressed2Code(compressed, bit_position, width, code))
				{
					error = "Unexpected end of Compressed2 stream";
					return false;
				}
				if (code == end_code)
				{
					return true;
				}
				if (code == clear_code)
				{
					reset_dictionary();
					width = 9;
					next_code = first_dictionary_code;
					next_width_threshold = 0x0200;
					have_previous = false;

					if (!ReadCompressed2Code(compressed, bit_position, width, code))
					{
						error = "Compressed2 CLEAR token has no following literal";
						return false;
					}
					if (code == end_code)
					{
						return true;
					}
					if (code > 0xFFU)
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
				if (code <= maximum_code && !dictionary[code].empty())
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
				if (have_previous && next_code <= maximum_code)
				{
					dictionary[next_code] = previous;
					dictionary[next_code].emplace_back(current.front());
				}
				++next_code;
				if (next_code >= next_width_threshold && width < 11U)
				{
					++width;
					next_width_threshold = static_cast<Uint16>(
						next_width_threshold << 1U
					);
				}
				previous = std::move(current);
				have_previous = true;
			}

			error = "Compressed2 validation exceeded the token limit";
			return false;
		}

		bool CanRead(const std::vector<Uint8>& data, Uint32 offset, std::size_t count)
		{
			return offset <= data.size() && count <= data.size() - offset;
		}

		Uint16 ReadBE16(const std::vector<Uint8>& data, Uint32 offset)
		{
			return static_cast<Uint16>(
				(static_cast<Uint16>(data[offset]) << 8U) |
				static_cast<Uint16>(data[offset + 1U])
			);
		}

		Sint16 ReadBE16Signed(const std::vector<Uint8>& data, Uint32 offset)
		{
			return static_cast<Sint16>(ReadBE16(data, offset));
		}

		Uint32 ReadBE32(const std::vector<Uint8>& data, Uint32 offset)
		{
			return
				(static_cast<Uint32>(data[offset]) << 24U) |
				(static_cast<Uint32>(data[offset + 1U]) << 16U) |
				(static_cast<Uint32>(data[offset + 2U]) << 8U) |
				static_cast<Uint32>(data[offset + 3U]);
		}


		bool ResolveBonusArtPointers(
			const SpinballROM& rom,
			BonusArtPointers& pointers,
			std::string& error
		)
		{
			// sub_FA588 contains exactly six PEA absolute-long instructions in
			// this range: two common art pointers followed by the four stage art
			// pointers. Discovering the operands keeps subsequent imports working
			// after a previous import has relocated one of the streams.
			constexpr Uint32 scan_begin = 0x000FA740;
			constexpr Uint32 scan_end = 0x000FA830;
			std::vector<Uint32> operands;
			if (rom.m_buffer.size() <= scan_begin)
			{
				error = "Bonus Stage loader is outside the ROM";
				return false;
			}

			const Uint32 bounded_end = static_cast<Uint32>(std::min<std::size_t>(
				rom.m_buffer.size(),
				static_cast<std::size_t>(scan_end)
			));
			for (Uint32 offset = scan_begin; offset + 6U <= bounded_end; ++offset)
			{
				if (rom.m_buffer[offset] == 0x48U &&
					rom.m_buffer[offset + 1U] == 0x79U)
				{
					operands.emplace_back(offset + 2U);
					offset += 5U;
				}
			}

			if (operands.size() != pointers.operand_offsets.size())
			{
				error = "Could not identify the six Bonus Stage art pointers in sub_FA588";
				return false;
			}

			for (std::size_t index = 0; index < operands.size(); ++index)
			{
				const Uint32 operand = operands[index];
				if (!CanRead(rom.m_buffer, operand, 4U))
				{
					error = "Bonus Stage art pointer operand is outside the ROM";
					return false;
				}
				pointers.operand_offsets[index] = operand;
				pointers.art_offsets[index] = ReadBE32(rom.m_buffer, operand);
				if (pointers.art_offsets[index] >= rom.m_buffer.size())
				{
					error = "Bonus Stage art pointer targets data outside the ROM";
					return false;
				}
			}
			return true;
		}

		std::string HexOffset(Uint32 value)
		{
			std::ostringstream stream;
			stream << "0x" << std::hex << std::uppercase << value;
			return stream.str();
		}

		bool LoadCompressedArt(
			const SpinballROM& rom,
			Uint32 rom_offset,
			Uint32 vram_offset,
			VirtualVram& vram,
			std::string& error
		)
		{
			TilesetEntry entry = TileSet::LoadFromROM_LZSSCompression(rom, rom_offset);
			if (entry.result.error_msg.has_value())
			{
				error = "Compressed2 decode failed at " + HexOffset(rom_offset) +
					": " + entry.result.error_msg.value();
				return false;
			}
			if (!entry.tileset || entry.tileset->uncompressed_data.empty())
			{
				error = "Compressed2 block at " + HexOffset(rom_offset) +
					" produced no data";
				return false;
			}

			const std::vector<Uint8>& source = entry.tileset->uncompressed_data;
			if (vram_offset >= vram.bytes.size() ||
				source.size() > vram.bytes.size() - vram_offset)
			{
				error = "Compressed2 block at " + HexOffset(rom_offset) +
					" does not fit in 64 KiB VRAM";
				return false;
			}

			std::copy(source.begin(), source.end(), vram.bytes.begin() + vram_offset);
			std::fill(
				vram.written.begin() + vram_offset,
				vram.written.begin() + vram_offset + source.size(),
				true
			);
			return true;
		}

		bool LoadEditableArtBlock(
			const SpinballROM& rom,
			const SpinballROM& reference_rom,
			Uint32 rom_offset,
			Uint32 reference_rom_offset,
			Uint32 pointer_operand_offset,
			Uint32 vram_offset,
			EditableArtBlock& block,
			std::string& error
		)
		{
			TilesetEntry entry = TileSet::LoadFromROM_LZSSCompression(rom, rom_offset);
			if (entry.result.error_msg.has_value())
			{
				error = "Compressed2 decode failed at " + HexOffset(rom_offset) +
					": " + entry.result.error_msg.value();
				return false;
			}
			if (!entry.tileset)
			{
				error = "Compressed2 block at " + HexOffset(rom_offset) +
					" could not be loaded";
				return false;
			}

			TilesetEntry reference_entry = TileSet::LoadFromROM_LZSSCompression(
				reference_rom,
				reference_rom_offset
			);
			if (reference_entry.result.error_msg.has_value() || !reference_entry.tileset)
			{
				error = "Could not measure original Compressed2 capacity at " +
					HexOffset(reference_rom_offset) + " in the reference ROM";
				return false;
			}

			block.rom_offset = rom_offset;
			block.pointer_operand_offset = pointer_operand_offset;
			block.vram_offset = vram_offset;
			block.original_capacity = reference_entry.tileset->compressed_size;
			block.current_compressed_size = entry.tileset->compressed_size;
			block.data = std::move(entry.tileset->uncompressed_data);
			block.touched = false;
			return true;
		}

		EditableArtBlock* FindArtBlockForVramByte(
			std::array<EditableArtBlock, 3>& blocks,
			std::size_t vram_byte_offset
		)
		{
			// Later uploads replace earlier VRAM contents. Search from the highest
			// upload address to reproduce that priority.
			for (auto iterator = blocks.rbegin(); iterator != blocks.rend(); ++iterator)
			{
				const std::size_t begin = iterator->vram_offset;
				const std::size_t end = begin + iterator->data.size();
				if (vram_byte_offset >= begin && vram_byte_offset < end)
				{
					return &*iterator;
				}
			}
			return nullptr;
		}

		bool WriteCompressedArtBlockInPlace(
			SpinballROM& rom,
			EditableArtBlock& block,
			std::size_t& compressed_size,
			std::size_t& bytes_beyond_original_span,
			std::string& error
		)
		{
			const std::vector<Uint8> compressed =
				EncodeCompressed2LzwStream(block.data);

			// The reference reader fetches a 24-bit window for every code. Add
			// temporary guard bytes for validation only; they are not part of the
			// stream written into the ROM.
			std::vector<Uint8> validation_stream = compressed;
			validation_stream.emplace_back(0);
			validation_stream.emplace_back(0);
			std::vector<Uint8> verified_output;
			if (!DecodeCompressed2Reference(validation_stream, verified_output, error))
			{
				error = "Reference Compressed2 validation failed: " + error;
				return false;
			}
			if (verified_output != block.data)
			{
				error = "Reference Compressed2 validation produced different art data";
				return false;
			}

			compressed_size = compressed.size();
			bytes_beyond_original_span = compressed_size > block.original_capacity
				? compressed_size - block.original_capacity
				: 0U;

			if (block.rom_offset > rom.m_buffer.size() ||
				compressed_size > rom.m_buffer.size() - block.rom_offset)
			{
				error = "The recompressed block would extend past the physical end of the ROM";
				return false;
			}

			// Strict in-place replacement: keep the original address and loader
			// pointer. If the stream is larger than its former span, this deliberately
			// overwrites the following bytes, as requested by the unrestricted mode.
			std::copy(
				compressed.begin(),
				compressed.end(),
				rom.m_buffer.begin() + block.rom_offset
			);
			return true;
		}


		void UpdateMegaDriveChecksum(SpinballROM& rom)
		{
			if (rom.m_buffer.size() < 0x190U)
			{
				return;
			}

			Uint32 checksum = 0;
			for (std::size_t offset = 0x200; offset < rom.m_buffer.size(); offset += 2U)
			{
				Uint16 word = static_cast<Uint16>(rom.m_buffer[offset]) << 8U;
				if (offset + 1U < rom.m_buffer.size())
				{
					word = static_cast<Uint16>(word | rom.m_buffer[offset + 1U]);
				}
				checksum = (checksum + word) & 0xFFFFU;
			}

			rom.m_buffer[0x18E] = static_cast<Uint8>((checksum >> 8U) & 0xFFU);
			rom.m_buffer[0x18F] = static_cast<Uint8>(checksum & 0xFFU);
		}

		bool ParseMapping(
			const std::vector<Uint8>& rom,
			Uint32 mapping_offset,
			ParsedMapping& out,
			std::string& error
		)
		{
			// Exact format used by sub_F7AFE:
			//   +0 word: piece count
			//   +2 word: X origin (subtracted from object X)
			//   +4 word: Y origin (subtracted from object Y)
			//   +6 repeated 8-byte pieces:
			//      word Y, word VDP size, word tile attributes, word X
			if (!CanRead(rom, mapping_offset, 6U))
			{
				error = "Mapping header is outside the ROM";
				return false;
			}

			const Uint16 piece_count = ReadBE16(rom, mapping_offset);
			if (piece_count == 0 || piece_count > 80)
			{
				error = "Implausible mapping piece count";
				return false;
			}

			const std::size_t mapping_size = 6U +
				static_cast<std::size_t>(piece_count) * 8U;
			if (!CanRead(rom, mapping_offset, mapping_size))
			{
				error = "Mapping pieces extend past the ROM";
				return false;
			}

			out = {};
			out.x_origin = ReadBE16Signed(rom, mapping_offset + 2U);
			out.y_origin = ReadBE16Signed(rom, mapping_offset + 4U);
			out.end_offset = mapping_offset + static_cast<Uint32>(mapping_size);
			out.pieces.reserve(piece_count);

			for (Uint16 index = 0; index < piece_count; ++index)
			{
				const Uint32 piece_offset = mapping_offset + 6U +
					static_cast<Uint32>(index) * 8U;
				const Uint8 size_code = static_cast<Uint8>(
					ReadBE16(rom, piece_offset + 2U) & 0x00FFU
				);

				MappingPiece piece;
				piece.y = ReadBE16Signed(rom, piece_offset);
				piece.width_in_tiles = static_cast<Uint8>(
					((size_code >> 2U) & 3U) + 1U
				);
				piece.height_in_tiles = static_cast<Uint8>((size_code & 3U) + 1U);
				piece.tile_attributes = ReadBE16(rom, piece_offset + 4U);
				piece.x = ReadBE16Signed(rom, piece_offset + 6U);
				out.pieces.emplace_back(piece);
			}
			return true;
		}

		bool IsTileRangeWritten(
			const VirtualVram& vram,
			int first_tile,
			int tile_count
		)
		{
			if (first_tile < 0 || tile_count <= 0)
			{
				return false;
			}
			const std::size_t byte_offset =
				static_cast<std::size_t>(first_tile) * TileSet::s_tile_total_bytes;
			const std::size_t byte_count =
				static_cast<std::size_t>(tile_count) * TileSet::s_tile_total_bytes;
			if (byte_offset > vram.bytes.size() ||
				byte_count > vram.bytes.size() - byte_offset)
			{
				return false;
			}
			return std::all_of(
				vram.written.begin() + byte_offset,
				vram.written.begin() + byte_offset + byte_count,
				[](bool value) { return value; }
			);
		}

		void CopyTileToPiece(
			const VirtualVram& vram,
			int tile_index,
			int tile_x,
			int tile_y,
			int piece_width_pixels,
			std::vector<Uint32>& pixels
		)
		{
			if (tile_index < 0)
			{
				return;
			}
			const std::size_t tile_byte_offset =
				static_cast<std::size_t>(tile_index) * TileSet::s_tile_total_bytes;
			if (tile_byte_offset > vram.bytes.size() ||
				TileSet::s_tile_total_bytes > vram.bytes.size() - tile_byte_offset)
			{
				return;
			}

			for (int pixel_y = 0; pixel_y < 8; ++pixel_y)
			{
				for (int byte_x = 0; byte_x < 4; ++byte_x)
				{
					const Uint8 packed = vram.bytes[
						tile_byte_offset + static_cast<std::size_t>(pixel_y * 4 + byte_x)
					];
					const int destination_y = tile_y * 8 + pixel_y;
					const int destination_x = tile_x * 8 + byte_x * 2;
					const std::size_t destination = static_cast<std::size_t>(
						destination_y * piece_width_pixels + destination_x
					);
					pixels[destination] = static_cast<Uint32>((packed >> 4U) & 0x0FU);
					pixels[destination + 1U] = static_cast<Uint32>(packed & 0x0FU);
				}
			}
		}

		std::shared_ptr<const Sprite> BuildObjectFrame(
			Uint32 mapping_offset,
			const ParsedMapping& mapping,
			Uint16 object_base_attributes,
			const VirtualVram& vram,
			std::string& warning
		)
		{
			auto sprite = std::make_shared<Sprite>();
			sprite->num_tiles = static_cast<Uint16>(mapping.pieces.size());
			sprite->rom_data.SetROMData(mapping_offset, mapping.end_offset);
			sprite->is_valid = true;

			Uint32 total_vdp_tiles = 0;
			for (std::size_t piece_index = 0;
				piece_index < mapping.pieces.size();
				++piece_index)
			{
				const MappingPiece& piece = mapping.pieces[piece_index];
				const Uint16 final_attributes = CombineTileAttributes(
					object_base_attributes,
					piece.tile_attributes
				);
				const int first_tile = static_cast<int>(final_attributes & kTileIndexMask);
				const int tile_count =
					static_cast<int>(piece.width_in_tiles) *
					static_cast<int>(piece.height_in_tiles);
				const bool tile_range_available =
					IsTileRangeWritten(vram, first_tile, tile_count);

				if (!tile_range_available && warning.empty())
				{
					warning = "Mapping " + HexOffset(mapping_offset) +
						" references tiles outside the uploaded VRAM art; "
						"missing pieces are transparent";
				}

				auto sprite_tile = std::make_shared<SpriteTile>();
				sprite_tile->x_offset = static_cast<Sint16>(
					GetDisplayedPieceX(piece, mapping, object_base_attributes)
				);
				sprite_tile->y_offset = static_cast<Sint16>(
					GetDisplayedPieceY(piece, mapping, object_base_attributes)
				);
				sprite_tile->x_size = static_cast<Uint8>(piece.width_in_tiles * 8U);
				sprite_tile->y_size = static_cast<Uint8>(piece.height_in_tiles * 8U);
				sprite_tile->blit_settings.flip_horizontal =
					(final_attributes & kHorizontalFlip) != 0;
				sprite_tile->blit_settings.flip_vertical =
					(final_attributes & kVerticalFlip) != 0;
				sprite_tile->pixel_data.assign(
					static_cast<std::size_t>(sprite_tile->x_size) * sprite_tile->y_size,
					0
				);

				if (tile_range_available)
				{
					// Mega Drive multi-tile sprites advance down each column first.
					int tile_cursor = first_tile;
					for (int tile_x = 0; tile_x < piece.width_in_tiles; ++tile_x)
					{
						for (int tile_y = 0; tile_y < piece.height_in_tiles; ++tile_y)
						{
							CopyTileToPiece(
								vram,
								tile_cursor++,
								tile_x,
								tile_y,
								sprite_tile->x_size,
								sprite_tile->pixel_data
							);
						}
					}
				}

				const Uint32 piece_rom_offset = mapping_offset + 6U +
					static_cast<Uint32>(piece_index) * 8U;
				sprite_tile->header_rom_data.SetROMData(
					piece_rom_offset,
					piece_rom_offset + 8U
				);
				sprite->sprite_tiles.emplace_back(std::move(sprite_tile));
				total_vdp_tiles += static_cast<Uint32>(tile_count);
			}

			if (total_vdp_tiles == 0 || total_vdp_tiles > std::numeric_limits<Uint16>::max())
			{
				warning = "Mapping uses an invalid number of VDP tiles";
				return nullptr;
			}
			sprite->num_vdp_tiles = static_cast<Uint16>(total_vdp_tiles);
			return sprite;
		}

		bool AddObjectFrame(
			const SpinballROM& rom,
			const VirtualVram& vram,
			const ObjectDefinition& definition,
			Uint32 animation_offset,
			Uint32 mapping_offset,
			Uint8 duration,
			Uint8 command,
			BonusStageAnimationState& state,
			BonusStageObjectFrames& object,
			std::vector<std::string>& warnings
		)
		{
			ParsedMapping mapping;
			std::string mapping_warning;
			if (!ParseMapping(rom.m_buffer, mapping_offset, mapping, mapping_warning))
			{
				warnings.emplace_back(
					object.name + " / " + state.name + ": skipped mapping " +
					HexOffset(mapping_offset) + ": " + mapping_warning
				);
				return false;
			}

			std::shared_ptr<const Sprite> sprite = BuildObjectFrame(
				mapping_offset,
				mapping,
				definition.base_tile_attributes,
				vram,
				mapping_warning
			);
			if (!sprite)
			{
				warnings.emplace_back(
					object.name + " / " + state.name + ": skipped mapping " +
					HexOffset(mapping_offset) + ": " + mapping_warning
				);
				return false;
			}
			if (!mapping_warning.empty())
			{
				warnings.emplace_back(
					object.name + " / " + state.name + ": " + mapping_warning
				);
			}

			BonusStageFrame frame;
			frame.animation_offset = animation_offset;
			frame.mapping_offset = mapping_offset;
			frame.duration = duration;
			frame.command = command;
			frame.sprite = std::move(sprite);
			state.frames.emplace_back(std::move(frame));
			return true;
		}

		void DecodeAnimationState(
			const SpinballROM& rom,
			const VirtualVram& vram,
			const ObjectDefinition& definition,
			const AnimationStateDefinition& state_definition,
			std::size_t maximum_entries,
			BonusStageObjectFrames& object,
			std::vector<std::string>& warnings
		)
		{
			BonusStageAnimationState state;
			state.name = state_definition.name;
			state.animation_root = state_definition.animation_root;
			state.is_static_mapping = false;

			std::unordered_set<Uint32> visited_entries;
			Uint32 current = state_definition.animation_root;
			for (std::size_t index = 0; index < maximum_entries; ++index)
			{
				if (!visited_entries.insert(current).second)
				{
					break; // Normal loop back to a previously decoded entry.
				}
				if (!CanRead(rom.m_buffer, current, 10U))
				{
					warnings.emplace_back(
						object.name + " / " + state.name + ": animation entry " +
						HexOffset(current) + " is outside the ROM"
					);
					break;
				}

				const Uint32 mapping_offset = ReadBE32(rom.m_buffer, current);
				const Uint8 duration = rom.m_buffer[current + 4U];
				const Uint8 command = rom.m_buffer[current + 5U];
				const Uint32 next = ReadBE32(rom.m_buffer, current + 6U);
				if (mapping_offset == 0 || mapping_offset >= rom.m_buffer.size())
				{
					warnings.emplace_back(
						object.name + " / " + state.name + ": animation entry " +
						HexOffset(current) + " has an invalid mapping pointer"
					);
					break;
				}

				// Do not deduplicate by mapping pointer. Two animation entries may use
				// the same mapping with different timing/commands, and two object
				// instances must remain independently selectable for later editing.
				AddObjectFrame(
					rom,
					vram,
					definition,
					current,
					mapping_offset,
					duration,
					command,
					state,
					object,
					warnings
				);

				if (next == 0 || next >= rom.m_buffer.size())
				{
					break;
				}
				current = next;
			}

			if (!state.frames.empty())
			{
				object.states.emplace_back(std::move(state));
			}
			else
			{
				warnings.emplace_back(
					object.name + " / " + state.name + ": no valid frame was decoded"
				);
			}
		}

		void DecodeObject(
			const SpinballROM& rom,
			const VirtualVram& vram,
			const ObjectDefinition& definition,
			std::size_t maximum_entries,
			BonusStageObjectFrames& object,
			std::vector<std::string>& warnings
		)
		{
			object.name = definition.name;
			object.object_ram_address = definition.object_ram_address;
			object.initial_mapping_offset = definition.initial_mapping;
			object.base_tile_attributes = definition.base_tile_attributes;
			object.is_common_object = definition.is_common_object;
			object.palette_line = static_cast<Uint8>(
				(definition.base_tile_attributes & kPaletteMask) >> 13U
			);
			object.high_priority =
				(definition.base_tile_attributes & kPriorityMask) != 0;
			object.flip_horizontal =
				(definition.base_tile_attributes & kHorizontalFlip) != 0;
			object.flip_vertical =
				(definition.base_tile_attributes & kVerticalFlip) != 0;

			// Keep the mapping written by the initialiser as its own state, even
			// when the first animation entry points to the same mapping.
			BonusStageAnimationState initial_state;
			initial_state.name = "Initial mapping";
			initial_state.animation_root = 0;
			initial_state.is_static_mapping = true;
			AddObjectFrame(
				rom,
				vram,
				definition,
				0,
				definition.initial_mapping,
				0,
				0,
				initial_state,
				object,
				warnings
			);
			if (!initial_state.frames.empty())
			{
				object.states.emplace_back(std::move(initial_state));
			}

			for (std::size_t index = 0; index < definition.state_count; ++index)
			{
				DecodeAnimationState(
					rom,
					vram,
					definition,
					definition.states[index],
					maximum_entries,
					object,
					warnings
				);
			}
		}
	}

	const char* BonusStageDecoder::GetStageName(BonusStageId stage)
	{
		const std::size_t index = static_cast<std::size_t>(stage);
		return index < std::size(kStages) ? kStages[index].name : "UNKNOWN";
	}

	bool BonusStageDecoder::IsBonusStageMappingOffset(Uint32 rom_offset)
	{
		if (rom_offset >= 0x000C8D8A && rom_offset < kCommonForegroundArt)
		{
			return true;
		}
		return std::any_of(
			std::begin(kStages),
			std::end(kStages),
			[rom_offset](const StageDefinition& definition)
			{
				return rom_offset >= definition.mapping_region_start &&
					rom_offset < definition.mapping_region_end;
			}
		);
	}

	BonusStageDecodeResult BonusStageDecoder::DecodeObjectFrames(
		const SpinballROM& rom,
		BonusStageId stage,
		std::size_t maximum_animation_entries
	)
	{
		BonusStageDecodeResult result;
		const std::size_t stage_index = static_cast<std::size_t>(stage);
		if (stage_index >= std::size(kStages))
		{
			result.error = "Unknown Bonus Stage id";
			return result;
		}
		if (rom.m_buffer.empty())
		{
			result.error = "No ROM is loaded";
			return result;
		}

		maximum_animation_entries = std::clamp<std::size_t>(
			maximum_animation_entries,
			1,
			65536
		);

		const StageDefinition& definition = kStages[stage_index];
		result.stage_name = definition.name;

		BonusArtPointers art_pointers;
		if (!ResolveBonusArtPointers(rom, art_pointers, result.error))
		{
			return result;
		}

		VirtualVram vram;
		if (!LoadCompressedArt(
			rom, art_pointers.art_offsets[0], kCommonBackgroundVram, vram, result.error
		) || !LoadCompressedArt(
			rom, art_pointers.art_offsets[1], kCommonForegroundVram, vram, result.error
		) || !LoadCompressedArt(
			rom, art_pointers.art_offsets[2U + stage_index], kStageArtVram, vram, result.error
		))
		{
			return result;
		}

		result.objects.reserve(definition.object_count + std::size(kCommonObjects));
		for (std::size_t index = 0; index < definition.object_count; ++index)
		{
			BonusStageObjectFrames object;
			DecodeObject(
				rom,
				vram,
				definition.objects[index],
				maximum_animation_entries,
				object,
				result.warnings
			);
			if (object.GetFrameCount() != 0)
			{
				result.objects.emplace_back(std::move(object));
			}
		}

		for (const ObjectDefinition& common_definition : kCommonObjects)
		{
			BonusStageObjectFrames object;
			DecodeObject(
				rom,
				vram,
				common_definition,
				maximum_animation_entries,
				object,
				result.warnings
			);
			if (object.GetFrameCount() != 0)
			{
				result.objects.emplace_back(std::move(object));
			}
		}

		if (result.GetFrameCount() == 0 && result.error.empty())
		{
			result.error = "No valid Bonus Stage Object Frame could be assembled";
		}
		return result;
	}

	BonusStageImportResult BonusStageDecoder::ImportIndexedImage(
		SpinballROM& rom,
		const SpinballROM& reference_rom,
		BonusStageId stage,
		Uint32 mapping_offset,
		Uint16 visual_tile_attributes,
		const std::vector<Uint8>& indexed_pixels,
		int image_width,
		int image_height
	)
	{
		BonusStageImportResult result;
		const std::size_t stage_index = static_cast<std::size_t>(stage);
		if (stage_index >= std::size(kStages))
		{
			result.message = "Unknown Bonus Stage id";
			return result;
		}

		ParsedMapping mapping;
		std::string error;
		if (!ParseMapping(rom.m_buffer, mapping_offset, mapping, error))
		{
			result.message = "Cannot import mapping " + HexOffset(mapping_offset) +
				": " + error;
			return result;
		}

		BonusArtPointers art_pointers;
		BonusArtPointers reference_art_pointers;
		if (!ResolveBonusArtPointers(rom, art_pointers, error))
		{
			result.message = error;
			return result;
		}
		if (!ResolveBonusArtPointers(reference_rom, reference_art_pointers, error))
		{
			result.message = "Reference ROM: " + error;
			return result;
		}

		std::array<EditableArtBlock, 3> art_blocks;
		if (!LoadEditableArtBlock(
			rom,
			reference_rom,
			art_pointers.art_offsets[0],
			reference_art_pointers.art_offsets[0],
			art_pointers.operand_offsets[0],
			kCommonBackgroundVram,
			art_blocks[0],
			error
		) || !LoadEditableArtBlock(
			rom,
			reference_rom,
			art_pointers.art_offsets[1],
			reference_art_pointers.art_offsets[1],
			art_pointers.operand_offsets[1],
			kCommonForegroundVram,
			art_blocks[1],
			error
		) || !LoadEditableArtBlock(
			rom,
			reference_rom,
			art_pointers.art_offsets[2U + stage_index],
			reference_art_pointers.art_offsets[2U + stage_index],
			art_pointers.operand_offsets[2U + stage_index],
			kStageArtVram,
			art_blocks[2],
			error
		))
		{
			result.message = error;
			return result;
		}

		int minimum_x = std::numeric_limits<int>::max();
		int minimum_y = std::numeric_limits<int>::max();
		for (const MappingPiece& piece : mapping.pieces)
		{
			minimum_x = std::min(
				minimum_x,
				GetDisplayedPieceX(piece, mapping, visual_tile_attributes)
			);
			minimum_y = std::min(
				minimum_y,
				GetDisplayedPieceY(piece, mapping, visual_tile_attributes)
			);
		}
		if (minimum_x == std::numeric_limits<int>::max() ||
			minimum_y == std::numeric_limits<int>::max())
		{
			result.message = "Mapping has no pieces";
			return result;
		}

		std::size_t written_pixels = 0;
		std::size_t unmapped_pixels = 0;
		for (const MappingPiece& piece : mapping.pieces)
		{
			const Uint16 final_attributes = CombineTileAttributes(
				visual_tile_attributes,
				piece.tile_attributes
			);
			const int first_tile = static_cast<int>(final_attributes & kTileIndexMask);
			const bool flip_horizontal = (final_attributes & kHorizontalFlip) != 0;
			const bool flip_vertical = (final_attributes & kVerticalFlip) != 0;
			const int piece_width = static_cast<int>(piece.width_in_tiles) * 8;
			const int piece_height = static_cast<int>(piece.height_in_tiles) * 8;
			const int piece_x = GetDisplayedPieceX(
				piece,
				mapping,
				visual_tile_attributes
			) - minimum_x;
			const int piece_y = GetDisplayedPieceY(
				piece,
				mapping,
				visual_tile_attributes
			) - minimum_y;

			for (int unflipped_y = 0; unflipped_y < piece_height; ++unflipped_y)
			{
				for (int unflipped_x = 0; unflipped_x < piece_width; ++unflipped_x)
				{
					const int displayed_x = piece_x + (
						flip_horizontal ? piece_width - 1 - unflipped_x : unflipped_x
					);
					const int displayed_y = piece_y + (
						flip_vertical ? piece_height - 1 - unflipped_y : unflipped_y
					);

					Uint8 palette_index = 0;
					if (displayed_x >= 0 && displayed_y >= 0 &&
						displayed_x < image_width && displayed_y < image_height)
					{
						const std::size_t source_index =
							static_cast<std::size_t>(displayed_y) *
							static_cast<std::size_t>(image_width) +
							static_cast<std::size_t>(displayed_x);
						if (source_index < indexed_pixels.size())
						{
							palette_index = static_cast<Uint8>(
								indexed_pixels[source_index] & 0x0FU
							);
						}
					}

					const int tile_x = unflipped_x / 8;
					const int tile_y = unflipped_y / 8;
					const int tile_in_piece =
						tile_x * static_cast<int>(piece.height_in_tiles) + tile_y;
					const int absolute_tile = first_tile + tile_in_piece;
					const std::size_t vram_byte_offset =
						static_cast<std::size_t>(absolute_tile) * TileSet::s_tile_total_bytes +
						static_cast<std::size_t>(unflipped_y % 8) * 4U +
						static_cast<std::size_t>((unflipped_x % 8) / 2);

					EditableArtBlock* block = FindArtBlockForVramByte(
						art_blocks,
						vram_byte_offset
					);
					if (!block)
					{
						++unmapped_pixels;
						continue;
					}

					const std::size_t relative_byte =
						vram_byte_offset - block->vram_offset;
					Uint8& packed = block->data[relative_byte];
					if ((unflipped_x & 1) == 0)
					{
						packed = static_cast<Uint8>(
							(packed & 0x0FU) | (palette_index << 4U)
						);
					}
					else
					{
						packed = static_cast<Uint8>((packed & 0xF0U) | palette_index);
					}
					block->touched = true;
					++written_pixels;
				}
			}
		}

		std::vector<std::string> rewrite_messages;
		for (EditableArtBlock& block : art_blocks)
		{
			if (!block.touched)
			{
				continue;
			}

			std::size_t compressed_size = 0;
			std::size_t bytes_beyond_original_span = 0;
			if (!WriteCompressedArtBlockInPlace(
				rom,
				block,
				compressed_size,
				bytes_beyond_original_span,
				error
			))
			{
				result.message = error;
				return result;
			}

			result.rewritten_art_offsets.emplace_back(block.rom_offset);
			std::ostringstream rewrite;
			const std::size_t remaining = compressed_size <= block.original_capacity
				? block.original_capacity - compressed_size
				: 0U;
			rewrite << HexOffset(block.rom_offset)
				<< " in place (original capacity " << block.original_capacity
				<< " bytes, current " << block.current_compressed_size
				<< " bytes, new " << compressed_size << " bytes";
			if (bytes_beyond_original_span == 0U)
			{
				rewrite << ", remaining " << remaining << " bytes";
			}
			else
			{
				rewrite << ", exceeds original capacity by "
					<< bytes_beyond_original_span << " bytes";
			}
			rewrite << ")";
			rewrite_messages.emplace_back(rewrite.str());
		}

		UpdateMegaDriveChecksum(rom);

		result.success = written_pixels != 0 && !result.rewritten_art_offsets.empty();
		if (result.success)
		{
			std::ostringstream message;
			message << "Imported " << written_pixels << " pixels into "
				<< result.rewritten_art_offsets.size()
				<< " Compressed2 block(s) at their original address";
			for (const std::string& rewrite : rewrite_messages)
			{
				message << "; " << rewrite;
			}
			if (unmapped_pixels != 0)
			{
				message << "; " << unmapped_pixels << " pixels referenced unloaded VRAM";
			}
			result.message = message.str();
		}
		else
		{
			result.message = "No referenced Bonus Stage tile could be modified";
		}
		return result;
	}
}
