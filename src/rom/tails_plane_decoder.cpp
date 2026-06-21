#include "rom/tails_plane_decoder.h"

#include "rom/compressed2_optimizer.h"

// tile.h must be complete before spinball_rom.h/tileset.h with GCC 15.
#include "rom/tile.h"
#include "rom/lzss_decompressor.h"
#include "rom/spinball_rom.h"
#include "rom/sprite.h"
#include "rom/sprite_tile.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

namespace spintool::rom
{
	namespace
	{
		constexpr Uint16 kTileIndexMask = 0x07FFU;
		constexpr Uint16 kHorizontalFlip = 0x0800U;
		constexpr Uint16 kVerticalFlip = 0x1000U;
		constexpr std::size_t kTileBytes = 32U;

		constexpr Uint32 kPlaneArtHeaderOffset = 0x000A3124U;
		// LoadUncOrComp2Tiles consumes FFFF, then gives the decoder A3126.
		constexpr Uint32 kPlaneCompressedStreamOffset = kPlaneArtHeaderOffset + 2U;
		constexpr Uint16 kPlaneBaseTile = 0x0000U;

		constexpr Uint32 kBlankDescriptor = 0x00099430U;

		// Profile-plane object lists used by the intro and ending scripts.
		constexpr Uint32 kSideBaseList = 0x0009AECAU;
		constexpr Uint32 kSidePropellerAList = 0x0009AF14U;
		constexpr Uint32 kSidePropellerBList = 0x0009AF22U;
		constexpr Uint32 kSidePilotList = 0x0009AF30U;
		constexpr Uint32 kSidePilotPartAList = 0x0009AF44U;
		constexpr Uint32 kSidePilotPartBList = 0x0009AF4CU;
		constexpr Uint32 kSideDetailList = 0x0009AF54U;
		constexpr Uint32 kSideTailAList = 0x0009AF62U;
		constexpr Uint32 kSideTailBList = 0x0009AF6AU;
		constexpr Uint32 kSideEndingDetailList = 0x0009B386U;

		// The attract-mode cutscene that runs immediately before an automatic
		// gameplay demo. It uses the same plane art but a separate scripted set
		// of mappings, including progressively smaller profile views.
		constexpr Uint32 kBeforeDemoScript = 0x0009A58AU;
		constexpr std::size_t kBeforeDemoFirstFrameId = 14U;
		constexpr std::size_t kBeforeDemoHiddenFrameFirstId = 21U;
		constexpr std::size_t kBeforeDemoHiddenFrameLastId = 23U;
		// The pre-demo script also controls water rings/wake and a separate Sonic
		// object. Only slots $02-$14 are the actual plane composition. Slots
		// $15-$2A are water effects and slot $46 is Sonic after he separates.
		constexpr Uint8 kBeforeDemoPlaneFirstSlot = 0x02U;
		constexpr Uint8 kBeforeDemoPlaneLastSlot = 0x14U;
		constexpr std::size_t kMaximumScriptOperations = 100000U;
		constexpr unsigned int kMaximumScriptDepth = 32U;

		// Front-plane descriptors used by sub_F2BB2 after pressing Start.
		constexpr Uint32 kFrontStaticDescriptor0 = 0x000A17CCU;
		constexpr Uint32 kFrontStaticDescriptor1 = 0x000A17D4U;
		constexpr Uint32 kFrontStaticDescriptor2 = 0x000A17DCU;
		constexpr Uint32 kFrontStaticDescriptor3 = 0x000A17E4U;
		constexpr Uint32 kFrontPropellerDescriptor = 0x000A17ECU;
		constexpr Uint32 kFrontTailDescriptor = 0x000A181CU;
		constexpr std::size_t kFrontPropellerFrames = 6U;

		constexpr Uint8 SceneBit(const TailsPlaneScene scene)
		{
			return static_cast<Uint8>(1U << static_cast<unsigned int>(scene));
		}

		struct PieceDescriptor
		{
			Uint32 rom_offset = 0;
			Uint8 width_in_tiles = 0;
			Uint8 height_in_tiles = 0;
			Sint16 x = 0;
			Sint16 y = 0;
			Uint16 attributes = 0;
		};

		struct PieceInstance
		{
			PieceDescriptor descriptor;
			Uint16 effective_attributes = 0;
			int position_x_pixels = 0;
			int position_y_pixels = 0;
			bool writable = true;
		};

		struct TableUse
		{
			Uint32 table_offset = 0;
			Uint8 flags_override = 0;
		};

		struct FrameDefinition
		{
			std::string name;
			std::string usage;
			std::size_t frame_id = 0;
			Uint8 scene_mask = 0;
			std::vector<PieceInstance> pieces;
			Uint16 base_tile = kPlaneBaseTile;
			Uint32 art_offset = kPlaneCompressedStreamOffset;
		};

		struct RasterImage
		{
			int width = 0;
			int height = 0;
			std::vector<Uint8> pixels;

			bool operator==(const RasterImage& rhs) const
			{
				return width == rhs.width && height == rhs.height && pixels == rhs.pixels;
			}
		};

		struct PixelBounds
		{
			int minimum_x = 0;
			int minimum_y = 0;
			int maximum_x = 0;
			int maximum_y = 0;

			[[nodiscard]] int Width() const { return maximum_x - minimum_x; }
			[[nodiscard]] int Height() const { return maximum_y - minimum_y; }
		};

		bool CanRead(const std::vector<Uint8>& data, const Uint32 offset, const std::size_t count)
		{
			return offset <= data.size() && count <= data.size() - offset;
		}

		Uint16 ReadBE16(const std::vector<Uint8>& data, const Uint32 offset)
		{
			return static_cast<Uint16>(
				(static_cast<Uint16>(data[offset]) << 8U) |
				static_cast<Uint16>(data[offset + 1U])
			);
		}

		Uint32 ReadBE32(const std::vector<Uint8>& data, const Uint32 offset)
		{
			return
				(static_cast<Uint32>(data[offset]) << 24U) |
				(static_cast<Uint32>(data[offset + 1U]) << 16U) |
				(static_cast<Uint32>(data[offset + 2U]) << 8U) |
				static_cast<Uint32>(data[offset + 3U]);
		}

		Sint16 ReadBE16Signed(const std::vector<Uint8>& data, const Uint32 offset)
		{
			return static_cast<Sint16>(ReadBE16(data, offset));
		}

		Uint16 ApplyObjectFlags(const Uint16 attributes, const Uint16 flags)
		{
			// Exact equivalent of sub_F40BE/sub_F41FE:
			//   andi.w #$F8F8,d0
			//   eor.b  d0,$E(a2)
			//   clr.b  d0
			//   eor.w  d0,$E(a2)
			const Uint16 filtered = static_cast<Uint16>(flags & 0xF8F8U);
			const Uint16 transformed = static_cast<Uint16>(
				((filtered & 0x00F8U) << 8U) |
				(filtered & 0xF800U)
			);
			return static_cast<Uint16>(attributes ^ transformed);
		}

		bool ParsePieceDescriptor(
			const std::vector<Uint8>& rom,
			const Uint32 offset,
			PieceDescriptor& output,
			std::string& error
		)
		{
			if (!CanRead(rom, offset, 8U))
			{
				error = "Object descriptor is outside the ROM";
				return false;
			}

			const Uint8 size_code = static_cast<Uint8>(rom[offset + 1U] & 0x0FU);
			output.rom_offset = offset;
			output.width_in_tiles = static_cast<Uint8>(((size_code >> 2U) & 3U) + 1U);
			output.height_in_tiles = static_cast<Uint8>((size_code & 3U) + 1U);
			output.x = ReadBE16Signed(rom, offset + 2U);
			output.y = ReadBE16Signed(rom, offset + 4U);
			output.attributes = ReadBE16(rom, offset + 6U);
			return true;
		}

		bool ApplyObjectTable(
			const std::vector<Uint8>& rom,
			const TableUse& table_use,
			std::map<Uint8, PieceInstance>& slots,
			std::string& error
		)
		{
			if (!CanRead(rom, table_use.table_offset, 2U))
			{
				error = "Plane object list is outside the ROM";
				return false;
			}
			const Uint8 first_slot = rom[table_use.table_offset];
			const Uint8 count = rom[table_use.table_offset + 1U];
			if (count == 0U || count > 64U ||
				!CanRead(rom, table_use.table_offset + 2U, static_cast<std::size_t>(count) * 6U))
			{
				error = "Plane object list has an invalid entry count";
				return false;
			}

			Uint32 cursor = table_use.table_offset + 2U;
			for (Uint8 index = 0U; index < count; ++index, cursor += 6U)
			{
				const Uint32 descriptor_offset = ReadBE32(rom, cursor);
				const Uint16 stored_flags = ReadBE16(rom, cursor + 4U);
				const Uint16 effective_flags = static_cast<Uint16>(
					(stored_flags & 0xFF00U) | table_use.flags_override
				);

				PieceDescriptor descriptor;
				if (!ParsePieceDescriptor(rom, descriptor_offset, descriptor, error))
				{
					std::ostringstream message;
					message << error << " at 0x" << std::hex << std::uppercase
						<< descriptor_offset;
					error = message.str();
					return false;
				}

				PieceInstance instance;
				instance.descriptor = descriptor;
				instance.effective_attributes = ApplyObjectFlags(
					descriptor.attributes,
					effective_flags
				);
				instance.writable = descriptor_offset != kBlankDescriptor;
				slots[static_cast<Uint8>(first_slot + index)] = instance;
			}
			return true;
		}


		struct ScriptObjectState
		{
			PieceInstance piece;
			Sint16 x_fixed = 0;
			Sint16 y_fixed = 0;
			Sint16 z_fixed = 0;
		};

		struct ObjectTableEntry
		{
			Uint8 slot = 0;
			Uint32 descriptor_offset = 0;
			Uint16 stored_flags = 0;
		};

		int FixedPointToPixels(const Sint16 value)
		{
			const int expanded = static_cast<int>(value);
			if (expanded >= 0)
			{
				return expanded / 16;
			}
			return -((-expanded + 15) / 16);
		}

		Sint16 AddWord(const Sint16 lhs, const Sint16 rhs)
		{
			return static_cast<Sint16>(static_cast<Uint16>(lhs) + static_cast<Uint16>(rhs));
		}

		bool IsRawBlankDescriptor(const std::vector<Uint8>& rom, const Uint32 offset)
		{
			if (offset == 0U || offset == kBlankDescriptor)
			{
				return true;
			}
			if (!CanRead(rom, offset, 8U))
			{
				return false;
			}
			return std::all_of(
				rom.begin() + static_cast<std::ptrdiff_t>(offset),
				rom.begin() + static_cast<std::ptrdiff_t>(offset + 8U),
				[](const Uint8 value) { return value == 0U; }
			);
		}

		bool ReadObjectTableEntries(
			const std::vector<Uint8>& rom,
			const Uint32 table_offset,
			std::vector<ObjectTableEntry>& entries,
			std::string& error
		)
		{
			entries.clear();
			if (!CanRead(rom, table_offset, 2U))
			{
				error = "Plane script object list is outside the ROM";
				return false;
			}
			const Uint8 first_slot = rom[table_offset];
			const Uint8 count = rom[table_offset + 1U];
			if (count == 0U || count > 64U ||
				!CanRead(rom, table_offset + 2U, static_cast<std::size_t>(count) * 6U))
			{
				error = "Plane script object list has an invalid entry count";
				return false;
			}

			entries.reserve(count);
			Uint32 cursor = table_offset + 2U;
			for (Uint8 index = 0U; index < count; ++index, cursor += 6U)
			{
				ObjectTableEntry entry;
				entry.slot = static_cast<Uint8>(first_slot + index);
				entry.descriptor_offset = ReadBE32(rom, cursor);
				entry.stored_flags = ReadBE16(rom, cursor + 4U);
				entries.emplace_back(entry);
			}
			return true;
		}

		bool SamePieceLayout(
			const std::vector<PieceInstance>& lhs,
			const std::vector<PieceInstance>& rhs
		)
		{
			if (lhs.size() != rhs.size())
			{
				return false;
			}
			for (std::size_t index = 0U; index < lhs.size(); ++index)
			{
				const PieceInstance& a = lhs[index];
				const PieceInstance& b = rhs[index];
				if (a.descriptor.rom_offset != b.descriptor.rom_offset ||
					a.effective_attributes != b.effective_attributes ||
					a.writable != b.writable)
				{
					return false;
				}
			}
			return true;
		}

		class BeforeDemoScriptCollector
		{
		public:
			BeforeDemoScriptCollector(
				const std::vector<Uint8>& rom,
				std::vector<FrameDefinition>& output
			)
				: m_rom(rom), m_output(output)
			{
			}

			bool Collect(std::string& error)
			{
				Uint32 end_offset = 0U;
				Sint8 return_value = 0;
				if (!Execute(kBeforeDemoScript, 0U, end_offset, return_value, error))
				{
					return false;
				}
				if (m_unique_frames == 0U)
				{
					error = "The pre-demo plane script produced no visible frame";
					return false;
				}
				return true;
			}

		private:
			bool Require(const Uint32 offset, const std::size_t count, std::string& error) const
			{
				if (CanRead(m_rom, offset, count))
				{
					return true;
				}
				std::ostringstream message;
				message << "Pre-demo plane script reads outside the ROM at 0x"
					<< std::hex << std::uppercase << offset;
				error = message.str();
				return false;
			}

			bool ParseDescriptorIntoState(
				ScriptObjectState& state,
				const Uint32 descriptor_offset,
				const Uint16 flags,
				std::string& error
			) const
			{
				if (descriptor_offset == 0U)
				{
					state.piece = {};
					state.piece.writable = false;
					return true;
				}

				PieceDescriptor descriptor;
				if (!ParsePieceDescriptor(m_rom, descriptor_offset, descriptor, error))
				{
					std::ostringstream message;
					message << error << " at 0x" << std::hex << std::uppercase
						<< descriptor_offset;
					error = message.str();
					return false;
				}
				state.piece.descriptor = descriptor;
				state.piece.effective_attributes = ApplyObjectFlags(
					descriptor.attributes,
					flags
				);
				state.piece.writable = !IsRawBlankDescriptor(m_rom, descriptor_offset);
				return true;
			}

			bool ApplyTable(
				const Uint8 flags_override,
				const Uint32 table_offset,
				const Sint16 x,
				const Sint16 y,
				const Sint16 z,
				const bool update_existing,
				std::string& error
			)
			{
				std::vector<ObjectTableEntry> entries;
				if (!ReadObjectTableEntries(m_rom, table_offset, entries, error))
				{
					return false;
				}
				for (const ObjectTableEntry& entry : entries)
				{
					ScriptObjectState state;
					const auto existing = m_slots.find(entry.slot);
					if (update_existing && existing != m_slots.end())
					{
						state = existing->second;
					}
					const Uint16 flags = static_cast<Uint16>(
						(entry.stored_flags & 0xFF00U) | flags_override
					);
					if (!ParseDescriptorIntoState(
						state,
						entry.descriptor_offset,
						flags,
						error
					))
					{
						return false;
					}
					if (update_existing)
					{
						state.x_fixed = AddWord(state.x_fixed, x);
						state.y_fixed = AddWord(state.y_fixed, y);
						state.z_fixed = AddWord(state.z_fixed, z);
					}
					else
					{
						state.x_fixed = x;
						state.y_fixed = y;
						state.z_fixed = z;
					}
					m_slots[entry.slot] = state;
				}
				return true;
			}

			bool DeleteTable(const Uint32 table_offset, std::string& error)
			{
				std::vector<ObjectTableEntry> entries;
				if (!ReadObjectTableEntries(m_rom, table_offset, entries, error))
				{
					return false;
				}
				for (const ObjectTableEntry& entry : entries)
				{
					m_slots.erase(entry.slot);
				}
				return true;
			}

			void CaptureFrame()
			{
				FrameDefinition definition;
				definition.usage = "Before automatic gameplay demo";
				definition.scene_mask = SceneBit(TailsPlaneScene::BeforeDemo);
				for (const auto& [slot, state] : m_slots)
				{
					if (slot < kBeforeDemoPlaneFirstSlot ||
						slot > kBeforeDemoPlaneLastSlot)
					{
						continue;
					}
					if (IsRawBlankDescriptor(m_rom, state.piece.descriptor.rom_offset))
					{
						continue;
					}
					PieceInstance piece = state.piece;
					piece.position_x_pixels = FixedPointToPixels(state.x_fixed);
					piece.position_y_pixels = FixedPointToPixels(state.y_fixed);
					definition.pieces.emplace_back(std::move(piece));
				}
				if (definition.pieces.empty())
				{
					return;
				}
				const auto duplicate = std::find_if(
					m_unique_piece_layouts.begin(),
					m_unique_piece_layouts.end(),
					[&definition](const std::vector<PieceInstance>& existing)
					{
						return SamePieceLayout(existing, definition.pieces);
					}
				);
				if (duplicate != m_unique_piece_layouts.end())
				{
					return;
				}

				m_unique_piece_layouts.emplace_back(definition.pieces);
				const std::size_t frame_id = kBeforeDemoFirstFrameId + m_unique_frames;
				++m_unique_frames;

				// These script states do not contain a Tails-plane frame to edit. Keep
				// their IDs reserved so the following frame IDs remain stable, but do
				// not expose them in the Tails Plane list.
				if (frame_id >= kBeforeDemoHiddenFrameFirstId &&
					frame_id <= kBeforeDemoHiddenFrameLastId)
				{
					return;
				}

				definition.frame_id = frame_id;
				definition.name = "Demo prelude - frame " +
					std::to_string(frame_id - kBeforeDemoFirstFrameId + 1U);
				m_output.emplace_back(std::move(definition));
			}

			bool Execute(
				Uint32 cursor,
				const unsigned int depth,
				Uint32& end_offset,
				Sint8& return_value,
				std::string& error
			)
			{
				if (depth > kMaximumScriptDepth)
				{
					error = "Pre-demo plane script recursion is too deep";
					return false;
				}

				while (m_operation_count < kMaximumScriptOperations)
				{
					++m_operation_count;
					if (!Require(cursor, 1U, error))
					{
						return false;
					}
					const Uint8 opcode = m_rom[cursor++];
					switch (opcode)
					{
					case 0U:
						if (!Require(cursor, 1U, error)) return false;
						return_value = static_cast<Sint8>(m_rom[cursor++]);
						end_offset = cursor;
						return true;

					case 1U:
					{
						if (!Require(cursor, 13U, error)) return false;
						const Uint8 slot = m_rom[cursor];
						const Uint16 flags = ReadBE16(m_rom, cursor + 1U);
						ScriptObjectState state;
						state.x_fixed = ReadBE16Signed(m_rom, cursor + 3U);
						state.y_fixed = ReadBE16Signed(m_rom, cursor + 5U);
						state.z_fixed = ReadBE16Signed(m_rom, cursor + 7U);
						if (!ParseDescriptorIntoState(
							state, ReadBE32(m_rom, cursor + 9U), flags, error
						)) return false;
						m_slots[slot] = state;
						cursor += 13U;
						break;
					}

					case 2U:
					case 8U:
					{
						if (!Require(cursor, 11U, error)) return false;
						const Uint8 override_flags = m_rom[cursor];
						const Uint32 table_offset = ReadBE32(m_rom, cursor + 1U);
						if (!ApplyTable(
							override_flags,
							table_offset,
							ReadBE16Signed(m_rom, cursor + 5U),
							ReadBE16Signed(m_rom, cursor + 7U),
							ReadBE16Signed(m_rom, cursor + 9U),
							opcode == 8U,
							error
						)) return false;
						cursor += 11U;
						break;
					}

					case 3U:
						if (!Require(cursor, 1U, error)) return false;
						m_slots.erase(m_rom[cursor++]);
						break;

					case 4U:
						if (!Require(cursor, 5U, error)) return false;
						if (!DeleteTable(ReadBE32(m_rom, cursor + 1U), error)) return false;
						cursor += 5U;
						break;

					case 5U:
					case 6U:
						break;

					case 7U:
					{
						if (!Require(cursor, 13U, error)) return false;
						const Uint8 slot = m_rom[cursor];
						ScriptObjectState state;
						const auto existing = m_slots.find(slot);
						if (existing != m_slots.end()) state = existing->second;
						const Uint32 descriptor_offset = ReadBE32(m_rom, cursor + 9U);
						if (descriptor_offset != 0U && !ParseDescriptorIntoState(
							state, descriptor_offset, ReadBE16(m_rom, cursor + 1U), error
						)) return false;
						state.x_fixed = AddWord(state.x_fixed, ReadBE16Signed(m_rom, cursor + 3U));
						state.y_fixed = AddWord(state.y_fixed, ReadBE16Signed(m_rom, cursor + 5U));
						state.z_fixed = AddWord(state.z_fixed, ReadBE16Signed(m_rom, cursor + 7U));
						m_slots[slot] = state;
						cursor += 13U;
						break;
					}

					case 9U:
						if (!Require(cursor, 1U, error)) return false;
						++cursor;
						CaptureFrame();
						break;

					case 10U:
					{
						if (!Require(cursor, 5U, error)) return false;
						const Uint32 subscript = ReadBE32(m_rom, cursor + 1U);
						cursor += 5U;
						Uint32 subscript_end = 0U;
						Sint8 subscript_return = 0;
						if (!Execute(
							subscript, depth + 1U, subscript_end, subscript_return, error
						)) return false;
						break;
					}

					case 11U:
					{
						if (!Require(cursor, 1U, error)) return false;
						const std::size_t argument_count = m_rom[cursor++];
						const std::size_t bytes = argument_count * 4U + 4U;
						if (!Require(cursor, bytes, error)) return false;
						cursor += static_cast<Uint32>(bytes);
						break;
					}

					case 12U:
					{
						if (!Require(cursor, 1U, error)) return false;
						const Uint8 repeat_count = m_rom[cursor++];
						if (repeat_count == 0U)
						{
							error = "Pre-demo plane script contains an empty repeat block";
							return false;
						}
						const Uint32 body = cursor;
						Uint32 loop_end = body;
						for (Uint8 iteration = 0U; iteration < repeat_count; ++iteration)
						{
							Sint8 loop_return = 0;
							Uint32 current_end = 0U;
							if (!Execute(
								body, depth + 1U, current_end, loop_return, error
							)) return false;
							if (iteration != 0U && current_end != loop_end)
							{
								error = "Pre-demo plane repeat block has inconsistent boundaries";
								return false;
							}
							loop_end = current_end;
						}
						cursor = loop_end;
						break;
					}

					case 13U:
						if (!Require(cursor, 3U, error)) return false;
						cursor += 3U;
						break;

					case 14U:
						if (!Require(cursor, 5U, error)) return false;
						cursor += 5U;
						break;

					case 15U:
						if (!Require(cursor, 7U, error)) return false;
						cursor += 7U;
						break;

					case 16U:
					{
						if (!Require(cursor, 1U, error)) return false;
						const Sint8 mode = static_cast<Sint8>(m_rom[cursor++]);
						const std::size_t bytes = mode == static_cast<Sint8>(-1)
							? 0U
							: (mode < 0 ? 8U : 4U);
						if (!Require(cursor, bytes, error)) return false;
						cursor += static_cast<Uint32>(bytes);
						break;
					}

					case 17U:
						if (!Require(cursor, 3U, error)) return false;
						cursor += 3U;
						break;

					default:
					{
						std::ostringstream message;
						message << "Unknown pre-demo plane script opcode "
							<< static_cast<unsigned int>(opcode)
							<< " at 0x" << std::hex << std::uppercase << (cursor - 1U);
						error = message.str();
						return false;
					}
					}
				}

				error = "Pre-demo plane script contains too many operations";
				return false;
			}

			const std::vector<Uint8>& m_rom;
			std::vector<FrameDefinition>& m_output;
			std::map<Uint8, ScriptObjectState> m_slots;
			std::vector<std::vector<PieceInstance>> m_unique_piece_layouts;
			std::size_t m_operation_count = 0U;
			std::size_t m_unique_frames = 0U;
		};

		bool BuildBeforeDemoFrameDefinitions(
			const std::vector<Uint8>& rom,
			std::vector<FrameDefinition>& output,
			std::string& error
		)
		{
			BeforeDemoScriptCollector collector(rom, output);
			return collector.Collect(error);
		}

		bool BuildSideFrameDefinition(
			const std::vector<Uint8>& rom,
			const std::size_t frame_id,
			std::string name,
			std::string usage,
			const Uint8 scene_mask,
			const std::vector<TableUse>& table_uses,
			FrameDefinition& output,
			std::string& error
		)
		{
			std::map<Uint8, PieceInstance> slots;
			for (const TableUse& table_use : table_uses)
			{
				if (!ApplyObjectTable(rom, table_use, slots, error))
				{
					std::ostringstream message;
					message << error << " at list 0x" << std::hex << std::uppercase
						<< table_use.table_offset;
					error = message.str();
					return false;
				}
			}

			output = {};
			output.frame_id = frame_id;
			output.name = std::move(name);
			output.usage = std::move(usage);
			output.scene_mask = scene_mask;
			for (const auto& [slot, piece] : slots)
			{
				(void)slot;
				if (piece.descriptor.rom_offset != kBlankDescriptor)
				{
					output.pieces.emplace_back(piece);
				}
			}
			if (output.pieces.empty())
			{
				error = "Plane profile frame contains no mapping piece";
				return false;
			}
			return true;
		}

		bool BuildFrontFrameDefinition(
			const std::vector<Uint8>& rom,
			const std::size_t frame_id,
			const std::size_t propeller_frame,
			FrameDefinition& output,
			std::string& error
		)
		{
			if (propeller_frame >= kFrontPropellerFrames)
			{
				error = "Front-plane propeller frame is outside the six game states";
				return false;
			}

			output = {};
			output.frame_id = frame_id;
			output.name = "Front view - propeller frame " +
				std::to_string(propeller_frame + 1U);
			output.usage = "After pressing Start";
			output.scene_mask = SceneBit(TailsPlaneScene::AfterStart);

			const std::array<Uint32, 6> descriptors =
			{
				kFrontStaticDescriptor0,
				kFrontStaticDescriptor1,
				kFrontStaticDescriptor2,
				kFrontStaticDescriptor3,
				kFrontPropellerDescriptor + static_cast<Uint32>(propeller_frame * 8U),
				kFrontTailDescriptor,
			};

			for (const Uint32 descriptor_offset : descriptors)
			{
				PieceDescriptor descriptor;
				if (!ParsePieceDescriptor(rom, descriptor_offset, descriptor, error))
				{
					return false;
				}

				PieceInstance normal;
				normal.descriptor = descriptor;
				normal.effective_attributes = descriptor.attributes;
				normal.writable = true;
				output.pieces.emplace_back(normal);

				PieceInstance mirrored = normal;
				mirrored.effective_attributes = ApplyObjectFlags(
					descriptor.attributes,
					0x0800U
				);
				// Both halves point to the same tile pixels. The importer collects
				// assignments first, so either half can be edited while conflicting
				// asymmetric edits are rejected instead of silently overwriting data.
				mirrored.writable = true;
				output.pieces.emplace_back(mirrored);
			}
			return true;
		}

		bool BuildAllFrameDefinitions(
			const std::vector<Uint8>& rom,
			std::vector<FrameDefinition>& output,
			std::vector<std::string>& warnings
		)
		{
			output.clear();
			const Uint8 intro = SceneBit(TailsPlaneScene::IntroBeforeTitle);
			const Uint8 after_start = SceneBit(TailsPlaneScene::AfterStart);
			const Uint8 before_demo = SceneBit(TailsPlaneScene::BeforeDemo);
			const Uint8 ending = SceneBit(TailsPlaneScene::EndingCutscene);
			(void)after_start;
			(void)before_demo;

			auto add_side = [&](const std::size_t id,
				std::string name,
				std::string usage,
				const Uint8 mask,
				std::vector<TableUse> tables)
			{
				FrameDefinition definition;
				std::string error;
				if (BuildSideFrameDefinition(
					rom,
					id,
					std::move(name),
					std::move(usage),
					mask,
					tables,
					definition,
					error
				))
				{
					output.emplace_back(std::move(definition));
				}
				else
				{
					warnings.emplace_back(error);
				}
			};

			// These two complete profile frames are genuinely shared by the intro
			// and ending. They are represented once and selected by a scene mask.
			add_side(
				0U,
				"Profile view - complete frame A",
				"Intro and ending",
				static_cast<Uint8>(intro | ending),
				{
					{kSideBaseList, 0x00U}, {kSidePropellerAList, 0x00U},
					{kSidePilotList, 0x00U}, {kSidePilotPartAList, 0x00U},
					{kSideDetailList, 0x00U}, {kSideTailAList, 0x00U},
				}
			);
			add_side(
				1U,
				"Profile view - complete frame B",
				"Intro and ending",
				static_cast<Uint8>(intro | ending),
				{
					{kSideBaseList, 0x00U}, {kSidePropellerBList, 0x00U},
					{kSidePilotList, 0x00U}, {kSidePilotPartBList, 0x00U},
					{kSideDetailList, 0x00U}, {kSideTailBList, 0x00U},
				}
			);

			// The intro turns the complete profile plane around by setting the
			// script override byte to 88. This flips both pixels and positions.
			add_side(
				2U,
				"Profile view - mirrored frame A",
				"Intro before title",
				intro,
				{
					{kSideBaseList, 0x88U}, {kSidePropellerAList, 0x88U},
					{kSidePilotList, 0x88U}, {kSidePilotPartAList, 0x88U},
					{kSideDetailList, 0x88U}, {kSideTailAList, 0x88U},
				}
			);
			add_side(
				3U,
				"Profile view - mirrored frame B",
				"Intro before title",
				intro,
				{
					{kSideBaseList, 0x88U}, {kSidePropellerBList, 0x88U},
					{kSidePilotList, 0x88U}, {kSidePilotPartBList, 0x88U},
					{kSideDetailList, 0x88U}, {kSideTailBList, 0x88U},
				}
			);

			// The ending first displays the profile plane without the upper pilot
			// overlay, then uses an alternate detail mapping later in the scene.
			add_side(
				4U,
				"Ending profile - reduced frame A",
				"Ending cutscene",
				ending,
				{
					{kSideBaseList, 0x80U}, {kSidePropellerAList, 0x80U},
					{kSideTailAList, 0x80U},
				}
			);
			add_side(
				5U,
				"Ending profile - reduced frame B",
				"Ending cutscene",
				ending,
				{
					{kSideBaseList, 0x80U}, {kSidePropellerBList, 0x80U},
					{kSideTailBList, 0x80U},
				}
			);
			add_side(
				6U,
				"Ending profile - alternate detail A",
				"Ending cutscene",
				ending,
				{
					{kSideBaseList, 0x80U}, {kSidePropellerAList, 0x80U},
					{kSidePilotList, 0x80U}, {kSidePilotPartAList, 0x80U},
					{kSideEndingDetailList, 0x80U}, {kSideTailAList, 0x80U},
				}
			);
			add_side(
				7U,
				"Ending profile - alternate detail B",
				"Ending cutscene",
				ending,
				{
					{kSideBaseList, 0x80U}, {kSidePropellerBList, 0x80U},
					{kSidePilotList, 0x80U}, {kSidePilotPartBList, 0x80U},
					{kSideEndingDetailList, 0x80U}, {kSideTailBList, 0x80U},
				}
			);

			for (std::size_t propeller = 0; propeller < kFrontPropellerFrames; ++propeller)
			{
				FrameDefinition definition;
				std::string error;
				if (BuildFrontFrameDefinition(
					rom,
					8U + propeller,
					propeller,
					definition,
					error
				))
				{
					output.emplace_back(std::move(definition));
				}
				else
				{
					warnings.emplace_back(error);
				}
			}

			std::string demo_error;
			if (!BuildBeforeDemoFrameDefinitions(rom, output, demo_error))
			{
				warnings.emplace_back(demo_error);
			}
			return !output.empty();
		}

		int DisplayX(const PieceInstance& piece)
		{
			const int width = static_cast<int>(piece.descriptor.width_in_tiles) * 8;
			const int local_x = (piece.effective_attributes & kHorizontalFlip) != 0U
				? -static_cast<int>(piece.descriptor.x) - width
				: static_cast<int>(piece.descriptor.x);
			return piece.position_x_pixels + local_x;
		}

		int DisplayY(const PieceInstance& piece)
		{
			const int height = static_cast<int>(piece.descriptor.height_in_tiles) * 8;
			const int local_y = (piece.effective_attributes & kVerticalFlip) != 0U
				? -static_cast<int>(piece.descriptor.y) - height
				: static_cast<int>(piece.descriptor.y);
			return piece.position_y_pixels + local_y;
		}

		PixelBounds ComputeBounds(const FrameDefinition& definition)
		{
			PixelBounds bounds;
			bounds.minimum_x = std::numeric_limits<int>::max();
			bounds.minimum_y = std::numeric_limits<int>::max();
			bounds.maximum_x = std::numeric_limits<int>::min();
			bounds.maximum_y = std::numeric_limits<int>::min();
			for (const PieceInstance& piece : definition.pieces)
			{
				const int x = DisplayX(piece);
				const int y = DisplayY(piece);
				const int width = piece.descriptor.width_in_tiles * 8;
				const int height = piece.descriptor.height_in_tiles * 8;
				bounds.minimum_x = std::min(bounds.minimum_x, x);
				bounds.minimum_y = std::min(bounds.minimum_y, y);
				bounds.maximum_x = std::max(bounds.maximum_x, x + width);
				bounds.maximum_y = std::max(bounds.maximum_y, y + height);
			}
			return bounds;
		}

		std::shared_ptr<SpriteTile> BuildSpriteTile(
			const PieceInstance& piece,
			const std::vector<Uint8>& art,
			const Uint16 base_tile,
			std::string& error
		)
		{
			const int width = static_cast<int>(piece.descriptor.width_in_tiles) * 8;
			const int height = static_cast<int>(piece.descriptor.height_in_tiles) * 8;
			const Uint16 absolute_tile = static_cast<Uint16>(
				piece.effective_attributes & kTileIndexMask
			);
			if (absolute_tile < base_tile)
			{
				error = "Object piece references a tile below its loaded art base";
				return nullptr;
			}
			const std::size_t first_tile = static_cast<std::size_t>(absolute_tile - base_tile);
			const std::size_t tile_count =
				static_cast<std::size_t>(piece.descriptor.width_in_tiles) *
				static_cast<std::size_t>(piece.descriptor.height_in_tiles);
			if (first_tile > art.size() / kTileBytes ||
				tile_count > art.size() / kTileBytes - first_tile)
			{
				error = "Object piece references tiles outside the decompressed art";
				return nullptr;
			}

			auto sprite_tile = std::make_shared<SpriteTile>();
			sprite_tile->x_size = static_cast<Uint8>(width);
			sprite_tile->y_size = static_cast<Uint8>(height);
			sprite_tile->x_offset = static_cast<Sint16>(DisplayX(piece));
			sprite_tile->y_offset = static_cast<Sint16>(DisplayY(piece));
			sprite_tile->blit_settings.flip_horizontal =
				(piece.effective_attributes & kHorizontalFlip) != 0U;
			sprite_tile->blit_settings.flip_vertical =
				(piece.effective_attributes & kVerticalFlip) != 0U;
			sprite_tile->pixel_data.assign(
				static_cast<std::size_t>(width) * static_cast<std::size_t>(height),
				0U
			);

			// Mega Drive multi-tile sprites advance vertically first, then move
			// to the next tile column.
			for (int tile_x = 0; tile_x < piece.descriptor.width_in_tiles; ++tile_x)
			{
				for (int tile_y = 0; tile_y < piece.descriptor.height_in_tiles; ++tile_y)
				{
					const std::size_t tile_index = first_tile +
						static_cast<std::size_t>(tile_x) * piece.descriptor.height_in_tiles +
						static_cast<std::size_t>(tile_y);
					const std::size_t tile_offset = tile_index * kTileBytes;
					for (int pixel_y = 0; pixel_y < 8; ++pixel_y)
					{
						for (int pixel_x = 0; pixel_x < 8; ++pixel_x)
						{
							const Uint8 packed = art[
								tile_offset + static_cast<std::size_t>(pixel_y) * 4U +
								static_cast<std::size_t>(pixel_x / 2)
							];
							const Uint8 colour = (pixel_x & 1) == 0
								? static_cast<Uint8>(packed >> 4U)
								: static_cast<Uint8>(packed & 0x0FU);
							const int destination_x = tile_x * 8 + pixel_x;
							const int destination_y = tile_y * 8 + pixel_y;
							sprite_tile->pixel_data[
								static_cast<std::size_t>(destination_y) * width +
								static_cast<std::size_t>(destination_x)
							] = colour;
						}
					}
				}
			}

			sprite_tile->header_rom_data.SetROMData(
				piece.descriptor.rom_offset,
				piece.descriptor.rom_offset + 8U
			);
			return sprite_tile;
		}

		std::shared_ptr<const Sprite> BuildSprite(
			const FrameDefinition& definition,
			const std::vector<Uint8>& art,
			std::string& error
		)
		{
			auto sprite = std::make_shared<Sprite>();
			Uint32 total_vdp_tiles = 0U;
			Uint32 minimum_descriptor = std::numeric_limits<Uint32>::max();
			Uint32 maximum_descriptor = 0U;
			for (const PieceInstance& piece : definition.pieces)
			{
				std::shared_ptr<SpriteTile> tile = BuildSpriteTile(
					piece,
					art,
					definition.base_tile,
					error
				);
				if (!tile)
				{
					return nullptr;
				}
				sprite->sprite_tiles.emplace_back(std::move(tile));
				total_vdp_tiles += static_cast<Uint32>(piece.descriptor.width_in_tiles) *
					piece.descriptor.height_in_tiles;
				minimum_descriptor = std::min(
					minimum_descriptor,
					piece.descriptor.rom_offset
				);
				maximum_descriptor = std::max(
					maximum_descriptor,
					piece.descriptor.rom_offset + 8U
				);
			}

			if (sprite->sprite_tiles.empty() ||
				sprite->sprite_tiles.size() > std::numeric_limits<Uint16>::max() ||
				total_vdp_tiles > std::numeric_limits<Uint16>::max())
			{
				error = "Complete Tails plane frame has an invalid tile count";
				return nullptr;
			}
			sprite->num_tiles = static_cast<Uint16>(sprite->sprite_tiles.size());
			sprite->num_vdp_tiles = static_cast<Uint16>(total_vdp_tiles);
			sprite->rom_data.SetROMData(minimum_descriptor, maximum_descriptor);
			sprite->is_valid = true;
			return sprite;
		}

		RasterImage RasteriseSprite(const Sprite& sprite)
		{
			RasterImage image;
			if (sprite.sprite_tiles.empty())
			{
				return image;
			}

			int minimum_x = std::numeric_limits<int>::max();
			int minimum_y = std::numeric_limits<int>::max();
			int maximum_x = std::numeric_limits<int>::min();
			int maximum_y = std::numeric_limits<int>::min();
			for (const std::shared_ptr<SpriteTile>& piece : sprite.sprite_tiles)
			{
				if (!piece)
				{
					continue;
				}
				minimum_x = std::min(minimum_x, static_cast<int>(piece->x_offset));
				minimum_y = std::min(minimum_y, static_cast<int>(piece->y_offset));
				maximum_x = std::max(
					maximum_x,
					static_cast<int>(piece->x_offset) + piece->x_size
				);
				maximum_y = std::max(
					maximum_y,
					static_cast<int>(piece->y_offset) + piece->y_size
				);
			}
			if (maximum_x <= minimum_x || maximum_y <= minimum_y)
			{
				return image;
			}

			image.width = maximum_x - minimum_x;
			image.height = maximum_y - minimum_y;
			image.pixels.assign(
				static_cast<std::size_t>(image.width) * image.height,
				0U
			);

			// Match Sprite::RenderToSurface: first mapping entry has priority.
			for (auto piece_it = sprite.sprite_tiles.rbegin();
				piece_it != sprite.sprite_tiles.rend();
				++piece_it)
			{
				const std::shared_ptr<SpriteTile>& piece = *piece_it;
				if (!piece)
				{
					continue;
				}
				for (int displayed_y = 0; displayed_y < piece->y_size; ++displayed_y)
				{
					for (int displayed_x = 0; displayed_x < piece->x_size; ++displayed_x)
					{
						const int source_x = piece->blit_settings.flip_horizontal
							? piece->x_size - 1 - displayed_x
							: displayed_x;
						const int source_y = piece->blit_settings.flip_vertical
							? piece->y_size - 1 - displayed_y
							: displayed_y;
						const Uint8 colour = static_cast<Uint8>(
							piece->pixel_data[
								static_cast<std::size_t>(source_y) * piece->x_size +
								static_cast<std::size_t>(source_x)
							] & 0x0FU
						);
						if (colour == 0U)
						{
							continue;
						}
						const int destination_x = piece->x_offset - minimum_x + displayed_x;
						const int destination_y = piece->y_offset - minimum_y + displayed_y;
						image.pixels[
							static_cast<std::size_t>(destination_y) * image.width +
							static_cast<std::size_t>(destination_x)
						] = colour;
					}
				}
			}
			return image;
		}

		void UpdateMegaDriveChecksum(SpinballROM& rom)
		{
			if (rom.m_buffer.size() < 0x190U)
			{
				return;
			}
			Uint32 checksum = 0U;
			for (std::size_t offset = 0x200U; offset < rom.m_buffer.size(); offset += 2U)
			{
				Uint16 word = static_cast<Uint16>(rom.m_buffer[offset]) << 8U;
				if (offset + 1U < rom.m_buffer.size())
				{
					word = static_cast<Uint16>(word | rom.m_buffer[offset + 1U]);
				}
				checksum = (checksum + word) & 0xFFFFU;
			}
			rom.m_buffer[0x18EU] = static_cast<Uint8>((checksum >> 8U) & 0xFFU);
			rom.m_buffer[0x18FU] = static_cast<Uint8>(checksum & 0xFFU);
		}

		bool ReadPiecePixel(
			const PieceInstance& piece,
			const std::vector<Uint8>& art,
			const Uint16 base_tile,
			const int displayed_local_x,
			const int displayed_local_y,
			Uint8& colour,
			std::size_t& byte_offset,
			bool& high_nibble,
			std::string& error
		)
		{
			const int piece_width = piece.descriptor.width_in_tiles * 8;
			const int piece_height = piece.descriptor.height_in_tiles * 8;
			if (displayed_local_x < 0 || displayed_local_x >= piece_width ||
				displayed_local_y < 0 || displayed_local_y >= piece_height)
			{
				return false;
			}

			const bool horizontal_flip =
				(piece.effective_attributes & kHorizontalFlip) != 0U;
			const bool vertical_flip =
				(piece.effective_attributes & kVerticalFlip) != 0U;
			const int unflipped_x = horizontal_flip
				? piece_width - 1 - displayed_local_x
				: displayed_local_x;
			const int unflipped_y = vertical_flip
				? piece_height - 1 - displayed_local_y
				: displayed_local_y;

			const Uint16 absolute_tile = static_cast<Uint16>(
				piece.effective_attributes & kTileIndexMask
			);
			if (absolute_tile < base_tile)
			{
				error = "Object piece references a tile below its loaded art base";
				return false;
			}
			const std::size_t first_tile = absolute_tile - base_tile;
			const std::size_t tile_count =
				static_cast<std::size_t>(piece.descriptor.width_in_tiles) *
				piece.descriptor.height_in_tiles;
			if (first_tile > art.size() / kTileBytes ||
				tile_count > art.size() / kTileBytes - first_tile)
			{
				error = "Object piece references tiles outside the decompressed art";
				return false;
			}

			const int tile_x = unflipped_x / 8;
			const int tile_y = unflipped_y / 8;
			const int pixel_x = unflipped_x & 7;
			const int pixel_y = unflipped_y & 7;
			const std::size_t tile_index = first_tile +
				static_cast<std::size_t>(tile_x) * piece.descriptor.height_in_tiles +
				static_cast<std::size_t>(tile_y);
			byte_offset = tile_index * kTileBytes +
				static_cast<std::size_t>(pixel_y) * 4U +
				static_cast<std::size_t>(pixel_x / 2);
			high_nibble = (pixel_x & 1) == 0;
			colour = high_nibble
				? static_cast<Uint8>(art[byte_offset] >> 4U)
				: static_cast<Uint8>(art[byte_offset] & 0x0FU);
			return true;
		}

		void WritePackedPixel(
			std::vector<Uint8>& art,
			const std::size_t byte_offset,
			const bool high_nibble,
			const Uint8 colour
		)
		{
			if (high_nibble)
			{
				art[byte_offset] = static_cast<Uint8>(
					(art[byte_offset] & 0x0FU) | ((colour & 0x0FU) << 4U)
				);
			}
			else
			{
				art[byte_offset] = static_cast<Uint8>(
					(art[byte_offset] & 0xF0U) | (colour & 0x0FU)
				);
			}
		}

		bool WriteFramePixelsToArt(
			const FrameDefinition& definition,
			std::vector<Uint8>& art,
			const std::vector<Uint8>& indexed_pixels,
			const int image_width,
			const int image_height,
			std::size_t& changed_pixel_count,
			std::size_t& conflicting_shared_pixels,
			std::string& error
		)
		{
			changed_pixel_count = 0U;
			conflicting_shared_pixels = 0U;
			if (image_width <= 0 || image_height <= 0)
			{
				error = "PNG dimensions are invalid";
				return false;
			}
			if (indexed_pixels.size() <
				static_cast<std::size_t>(image_width) * image_height)
			{
				error = "PNG indexed-pixel buffer is smaller than its dimensions";
				return false;
			}

			const PixelBounds bounds = ComputeBounds(definition);
			if (image_width != bounds.Width() || image_height != bounds.Height())
			{
				std::ostringstream message;
				message << "PNG must be exactly " << bounds.Width() << 'x' << bounds.Height()
					<< " pixels for this assembled frame (received "
					<< image_width << 'x' << image_height << ')';
				error = message.str();
				return false;
			}

			struct Candidate
			{
				std::size_t byte_offset = 0U;
				bool high_nibble = false;
				Uint8 current_colour = 0U;
			};
			struct PixelAssignment
			{
				std::size_t byte_offset = 0U;
				bool high_nibble = false;
				Uint8 colour = 0U;
			};
			std::map<std::size_t, PixelAssignment> changed_pixels;

			// The first mapping entry is visually on top. Resolve every final-image
			// pixel to its real source nibble without changing art during the scan.
			// A source nibble may be displayed more than once (notably the mirrored
			// front view), so identical edits are merged and incompatible edits fail.
			for (int image_y = 0; image_y < image_height; ++image_y)
			{
				for (int image_x = 0; image_x < image_width; ++image_x)
				{
					std::optional<Candidate> transparent_fallback;
					std::optional<Candidate> owner;

					for (const PieceInstance& piece : definition.pieces)
					{
						if (!piece.writable)
						{
							continue;
						}
						const int local_x = image_x + bounds.minimum_x - DisplayX(piece);
						const int local_y = image_y + bounds.minimum_y - DisplayY(piece);
						Uint8 current_colour = 0U;
						std::size_t byte_offset = 0U;
						bool high_nibble = false;
						if (!ReadPiecePixel(
							piece,
							art,
							definition.base_tile,
							local_x,
							local_y,
							current_colour,
							byte_offset,
							high_nibble,
							error
						))
						{
							if (!error.empty())
							{
								return false;
							}
							continue;
						}

						const Candidate candidate{byte_offset, high_nibble, current_colour};
						if (!transparent_fallback.has_value())
						{
							transparent_fallback = candidate;
						}
						if (current_colour != 0U)
						{
							owner = candidate;
							break;
						}
					}

					const std::optional<Candidate>& selected = owner.has_value()
						? owner
						: transparent_fallback;
					if (!selected.has_value())
					{
						continue;
					}
					const Uint8 imported_colour = static_cast<Uint8>(
						indexed_pixels[
							static_cast<std::size_t>(image_y) * image_width +
							static_cast<std::size_t>(image_x)
						] & 0x0FU
					);
					if (imported_colour == selected->current_colour)
					{
						continue;
					}

					const std::size_t source_key = selected->byte_offset * 2U +
						(selected->high_nibble ? 0U : 1U);
					const auto existing = changed_pixels.find(source_key);
					if (existing != changed_pixels.end())
					{
						if (existing->second.colour != imported_colour)
						{
							// Shared or mirrored appearances cannot hold two colours. Keep the
							// first actual edit instead of rejecting the complete PNG.
							++conflicting_shared_pixels;
						}
						continue;
					}
					changed_pixels.emplace(source_key, PixelAssignment{
						selected->byte_offset,
						selected->high_nibble,
						imported_colour
					});
				}
			}

			for (const auto& [source_key, assignment] : changed_pixels)
			{
				(void)source_key;
				WritePackedPixel(
					art,
					assignment.byte_offset,
					assignment.high_nibble,
					assignment.colour
				);
			}
			changed_pixel_count = changed_pixels.size();
			return true;
		}

	}

	const std::vector<TailsPlaneSceneInfo>& TailsPlaneDecoder::GetSceneInfos()
	{
		static const std::vector<TailsPlaneSceneInfo> scenes =
		{
			{
				TailsPlaneScene::IntroBeforeTitle,
				"Intro before title",
				"sub_F2554 -> unk_9B0DA -> lists 9AECA/9AFxx",
				"Profile plane: two animation states in both directions."
			},
			{
				TailsPlaneScene::AfterStart,
				"Launch sequence after Start",
				"sub_F2652(1) -> sub_F2808 -> sub_F2BB2 -> off_99344",
				"Front plane: six propeller states, each assembled from six source pieces and six hardware mirrors."
			},
			{
				TailsPlaneScene::BeforeDemo,
				"Before gameplay demo",
				"sub_F2C82 -> sub_F2E12 -> unk_9A58A",
				"Attract-mode plane: scripted profile animation with progressively smaller and more distant states; water effects and the separate Sonic object are excluded."
			},
			{
				TailsPlaneScene::EndingCutscene,
				"Ending cutscene",
				"unk_9B75C -> lists 9AECA/9AFxx/9B386",
				"Profile plane: shared complete states plus ending-only reduced and alternate-detail states."
			},
		};
		return scenes;
	}

	TailsPlaneDecodeResult TailsPlaneDecoder::Decode(const SpinballROM& rom)
	{
		TailsPlaneDecodeResult result;
		if (!CanRead(rom.m_buffer, kPlaneArtHeaderOffset, 2U) ||
			ReadBE16(rom.m_buffer, kPlaneArtHeaderOffset) != 0xFFFFU)
		{
			result.error = "Tails plane Compressed2 marker FFFF is missing at 0xA3124";
			return result;
		}

		const LZSSDecompressionResult decompressed =
			LZSSDecompressor::DecompressDataRefactored(
				rom.m_buffer,
				kPlaneCompressedStreamOffset
			);
		if (decompressed.error_msg.has_value())
		{
			result.error = "Could not decompress Tails plane art at 0xA3126: " +
				*decompressed.error_msg;
			return result;
		}
		if (decompressed.uncompressed_data.size() <= 2U)
		{
			result.error = "Tails plane art decompressed to an empty tile payload";
			return result;
		}

		const std::vector<Uint8> art(
			decompressed.uncompressed_data.begin() + 2,
			decompressed.uncompressed_data.end()
		);
		std::vector<FrameDefinition> definitions;
		BuildAllFrameDefinitions(rom.m_buffer, definitions, result.warnings);

		std::vector<RasterImage> unique_rasters;
		for (const FrameDefinition& definition : definitions)
		{
			std::string error;
			std::shared_ptr<const Sprite> sprite = BuildSprite(definition, art, error);
			if (!sprite)
			{
				result.warnings.emplace_back(definition.name + ": " + error);
				continue;
			}

			const RasterImage raster = RasteriseSprite(*sprite);
			std::optional<std::size_t> duplicate_index;
			for (std::size_t index = 0U; index < unique_rasters.size(); ++index)
			{
				// Deduplicate only inside the same selected scene. Identical images
				// belonging to another scene must remain separate entries, otherwise
				// frames from the other three groups appear in the current list.
				if (unique_rasters[index] == raster &&
					(result.frames[index].scene_mask & definition.scene_mask) != 0U)
				{
					duplicate_index = index;
					break;
				}
			}
			if (duplicate_index.has_value())
			{
				result.frames[*duplicate_index].scene_mask = static_cast<Uint8>(
					result.frames[*duplicate_index].scene_mask | definition.scene_mask
				);
				result.warnings.emplace_back(
					"Visually identical to another frame in the same scene and was merged"
				);
				continue;
			}

			unique_rasters.emplace_back(raster);
			TailsPlaneFrame frame;
			frame.name = definition.name;
			frame.usage = definition.usage;
			frame.frame_id = definition.frame_id;
			frame.scene_mask = definition.scene_mask;
			frame.sprite = std::move(sprite);
			result.frames.emplace_back(std::move(frame));
		}

		if (result.frames.empty())
		{
			result.error = "No complete frame could be assembled";
		}
		return result;
	}

	TailsPlaneImportResult TailsPlaneDecoder::ImportIndexedImage(
		SpinballROM& rom,
		const SpinballROM& reference_rom,
		const std::size_t frame_id,
		const std::vector<Uint8>& indexed_pixels,
		const int image_width,
		const int image_height
	)
	{
		TailsPlaneImportResult result;
		if (!CanRead(rom.m_buffer, kPlaneArtHeaderOffset, 2U) ||
			ReadBE16(rom.m_buffer, kPlaneArtHeaderOffset) != 0xFFFFU)
		{
			result.message = "Compressed2 marker FFFF is missing at 0xA3124";
			return result;
		}
		if (!CanRead(reference_rom.m_buffer, kPlaneArtHeaderOffset, 2U) ||
			ReadBE16(reference_rom.m_buffer, kPlaneArtHeaderOffset) != 0xFFFFU)
		{
			result.message = "Reference ROM has no Compressed2 marker at 0xA3124";
			return result;
		}

		std::vector<FrameDefinition> definitions;
		std::vector<std::string> warnings;
		BuildAllFrameDefinitions(rom.m_buffer, definitions, warnings);
		const auto definition_it = std::find_if(
			definitions.begin(),
			definitions.end(),
			[frame_id](const FrameDefinition& definition)
			{
				return definition.frame_id == frame_id;
			}
		);
		if (definition_it == definitions.end())
		{
			result.message = "Selected frame ID is not available";
			return result;
		}
		const FrameDefinition& definition = *definition_it;

		// Decode the working stream with the same exact Compressed2 decoder used
		// for validation and recompression.  The old path used the legacy game
		// emulation decoder for the working ROM and the portable decoder for the
		// reference ROM, then required both payload sizes to match.  That could
		// reject every later import after a ROM had already been saved by SpinTool,
		// even though the selected frame and its mapped tiles were still valid.
		std::vector<Uint8> tile_art;
		std::size_t current_stream_size = 0U;
		std::string error;
		if (!Compressed2Optimizer::Decode(
			rom.m_buffer,
			definition.art_offset,
			tile_art,
			error,
			&current_stream_size,
			nullptr
		))
		{
			result.message = "Could not decompress working art: " + error;
			return result;
		}
		if (tile_art.empty())
		{
			result.message = "Working art has no tile payload";
			return result;
		}

		std::vector<Uint8> reference_tile_art;
		std::size_t capacity = 0U;
		if (!Compressed2Optimizer::Decode(
			reference_rom.m_buffer,
			definition.art_offset,
			reference_tile_art,
			error,
			&capacity,
			nullptr
		))
		{
			result.message = "Could not determine exact reference capacity: " +
				error;
			return result;
		}
		if (definition.art_offset > reference_rom.m_buffer.size() ||
			capacity > reference_rom.m_buffer.size() - definition.art_offset)
		{
			result.message = "Reference compressed-art block extends outside the ROM";
			return result;
		}
		// The reference stream defines only the maximum writable span.  The
		// editable payload must come from the current working ROM: it may already
		// contain valid previous edits and therefore does not have to be byte-for-
		// byte or size-for-size identical to the reference payload.
		if (definition.art_offset > rom.m_buffer.size() ||
			current_stream_size > rom.m_buffer.size() - definition.art_offset)
		{
			result.message = "Working compressed-art block extends outside the ROM";
			return result;
		}
		const std::vector<Uint8> original_stream(
			rom.m_buffer.begin() + definition.art_offset,
			rom.m_buffer.begin() + definition.art_offset + current_stream_size
		);
		std::size_t changed_pixel_count = 0U;
		std::size_t conflicting_shared_pixels = 0U;
		if (!WriteFramePixelsToArt(
			definition,
			tile_art,
			indexed_pixels,
			image_width,
			image_height,
			changed_pixel_count,
			conflicting_shared_pixels,
			error
		))
		{
			result.message = error;
			return result;
		}
		if (changed_pixel_count == 0U)
		{
			result.success = true;
			result.changed = false;
			result.message =
				"The PNG is identical to the current frame; the ROM was not modified.";
			return result;
		}

		const Compressed2CompressionResult compression =
			Compressed2Optimizer::Compress(tile_art, original_stream);
		std::vector<Uint8> verified_art;
		std::size_t consumed_size = 0U;
		if (!Compressed2Optimizer::Decode(
			compression.data,
			0U,
			verified_art,
			error,
			&consumed_size,
			nullptr
		))
		{
			result.message = "Optimized Compressed2 validation failed: " + error;
			return result;
		}
		if (verified_art != tile_art || consumed_size != compression.data.size())
		{
			result.message = "Optimized Compressed2 validation produced different art";
			return result;
		}

		if (compression.data.size() > capacity)
		{
			std::ostringstream message;
			message << "Import refused before writing the ROM. Modified art needs "
				<< compression.data.size() << " bytes after optimized compression, but "
				<< "the original block only has " << capacity << " bytes ("
				<< compression.data.size() - capacity << " bytes too large; basic "
				<< "compression was " << compression.baseline_size << " bytes).";
			result.message = message.str();
			return result;
		}
		if (definition.art_offset > rom.m_buffer.size() ||
			capacity > rom.m_buffer.size() - definition.art_offset)
		{
			result.message = "Compressed-art block extends outside the working ROM";
			return result;
		}

		std::copy(
			compression.data.begin(),
			compression.data.end(),
			rom.m_buffer.begin() + definition.art_offset
		);
		std::fill(
			rom.m_buffer.begin() + definition.art_offset + compression.data.size(),
			rom.m_buffer.begin() + definition.art_offset + capacity,
			0U
		);
		UpdateMegaDriveChecksum(rom);

		result.success = true;
		result.changed = true;
		std::ostringstream message;
		message << changed_pixel_count
			<< " source pixels changed; optimized size " << compression.data.size()
			<< "/" << capacity << " bytes; strategy " << compression.strategy;
		if (compression.baseline_size > compression.data.size())
		{
			message << "; saved "
				<< compression.baseline_size - compression.data.size()
				<< " bytes versus basic compression";
		}
		if (conflicting_shared_pixels != 0U)
		{
			message << "; " << conflicting_shared_pixels
				<< " conflicting shared appearances kept their first edited colour";
		}
		message << ". All scenes referencing these source tiles use the change.";
		result.message = message.str();
		return result;
	}
}
