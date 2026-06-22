#include "rom/title_screen_decoder.h"

#include "rom/compressed2_optimizer.h"

// tile.h must be complete before spinball_rom.h/tileset.h with GCC 15.
#include "rom/tile.h"
#include "rom/spinball_rom.h"
#include "rom/palette.h"
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

		constexpr Uint32 kTitleArtHeaderOffset = 0x0009D102U;
		constexpr Uint32 kTitleCompressedStreamOffset = kTitleArtHeaderOffset + 2U;
		constexpr Uint16 kTitleBaseTile = 0x0000U;
		constexpr std::array<Uint32, 7> kTitleSonicObjectTables
		{
			0x00099440U,
			0x000994BAU,
			0x00099534U,
			0x000995AEU,
			0x00099628U,
			0x000996A2U,
			0x0009971CU,
		};
		// The title screen stores each major visual element in its own object table.
		// Their order in ROM does not match their visual/name order on screen.
		constexpr Uint32 kTitleLogoSonicObjectTable = 0x00099796U;
		constexpr Uint32 kTitleLogoTheHedgehogObjectTable = 0x00099816U;
		constexpr Uint32 kTitleLogoSpinballObjectTable = 0x00099848U;
		// This object table is another Sonic animation composition, not the
		// large pinball support visible underneath him.
		constexpr Uint32 kTitleAdditionalSonicObjectTable = 0x000998A4U;
		// The support/bumper itself is stored as the title-screen foreground
		// tile layout and uses the same shared title-screen graphics block.
		constexpr Uint32 kTitleBumperRingTileLayout = 0x0009C05AU;
		constexpr Uint32 kTitlePaletteDataOffset = 0x0009BD3AU;
		constexpr Uint32 kBlankDescriptor = 0x00099430U;
		constexpr int kMaximumFrameDimension = 2048;

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

		struct FrameDefinition
		{
			TitleScreenCategory category = TitleScreenCategory::SONIC;
			std::string name;
			std::string usage;
			std::size_t frame_id = 0;
			std::vector<PieceInstance> pieces;
			Uint16 base_tile = kTitleBaseTile;
			Uint32 art_offset = kTitleCompressedStreamOffset;
		};


		struct ObjectTableEntry
		{
			Uint8 slot = 0;
			Uint32 descriptor_offset = 0;
			Uint16 stored_flags = 0;
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
			const Uint16 filtered = static_cast<Uint16>(flags & 0xF8F8U);
			const Uint16 transformed = static_cast<Uint16>(
				((filtered & 0x00F8U) << 8U) |
				(filtered & 0xF800U)
			);
			return static_cast<Uint16>(attributes ^ transformed);
		}

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

		bool ParsePieceDescriptor(
			const std::vector<Uint8>& rom,
			const Uint32 offset,
			PieceDescriptor& output,
			std::string& error
		)
		{
			if (!CanRead(rom, offset, 8U))
			{
				error = "Title-screen object descriptor is outside the ROM";
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
				error = "Title-screen object list is outside the ROM";
				return false;
			}
			const Uint8 first_slot = rom[table_offset];
			const Uint8 count = rom[table_offset + 1U];
			if (count == 0U || count > 96U ||
				!CanRead(rom, table_offset + 2U, static_cast<std::size_t>(count) * 6U))
			{
				error = "Title-screen object list has an invalid entry count";
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

		bool AppendObjectTableFrame(
			const std::vector<Uint8>& rom,
			const Uint32 table_offset,
			const TitleScreenCategory category,
			std::string name,
			std::string usage,
			const std::size_t frame_id,
			std::vector<FrameDefinition>& output,
			std::string& error,
			const Uint32 first_descriptor = 0U,
			const Uint32 last_descriptor = std::numeric_limits<Uint32>::max()
		)
		{
			std::vector<ObjectTableEntry> entries;
			if (!ReadObjectTableEntries(rom, table_offset, entries, error))
			{
				return false;
			}

			FrameDefinition definition;
			definition.category = category;
			definition.name = std::move(name);
			definition.usage = std::move(usage);
			definition.frame_id = frame_id;
			for (const ObjectTableEntry& entry : entries)
			{
				if (entry.descriptor_offset < first_descriptor ||
					entry.descriptor_offset > last_descriptor ||
					IsRawBlankDescriptor(rom, entry.descriptor_offset))
				{
					continue;
				}

				PieceDescriptor descriptor;
				if (!ParsePieceDescriptor(rom, entry.descriptor_offset, descriptor, error))
				{
					return false;
				}
				PieceInstance piece;
				piece.descriptor = descriptor;
				piece.effective_attributes = ApplyObjectFlags(
					descriptor.attributes,
					entry.stored_flags
				);
				piece.position_x_pixels = 0;
				piece.position_y_pixels = 0;
				piece.writable = true;
				definition.pieces.emplace_back(std::move(piece));
			}
			if (definition.pieces.empty())
			{
				error = definition.name + " contains no editable title-screen pieces";
				return false;
			}
			output.emplace_back(std::move(definition));
			return true;
		}

		bool AppendTileLayoutFrame(
			const std::vector<Uint8>& rom,
			const Uint32 layout_offset,
			const TitleScreenCategory category,
			std::string name,
			std::string usage,
			const std::size_t frame_id,
			std::vector<FrameDefinition>& output,
			std::string& error
		)
		{
			if (!CanRead(rom, layout_offset, 4U))
			{
				error = "Title-screen tile layout header is outside the ROM";
				return false;
			}

			const Uint16 width_in_tiles = ReadBE16(rom, layout_offset);
			const Uint16 height_in_tiles = ReadBE16(rom, layout_offset + 2U);
			if (width_in_tiles == 0U || height_in_tiles == 0U ||
				width_in_tiles > 128U || height_in_tiles > 128U)
			{
				error = "Title-screen bumper layout has invalid dimensions";
				return false;
			}

			const std::size_t cell_count =
				static_cast<std::size_t>(width_in_tiles) * height_in_tiles;
			const Uint32 cells_offset = layout_offset + 4U;
			if (!CanRead(rom, cells_offset, cell_count * sizeof(Uint16)))
			{
				error = "Title-screen bumper layout data is outside the ROM";
				return false;
			}

			FrameDefinition definition;
			definition.category = category;
			definition.name = std::move(name);
			definition.usage = std::move(usage);
			definition.frame_id = frame_id;
			definition.pieces.reserve(cell_count);

			for (Uint16 y = 0U; y < height_in_tiles; ++y)
			{
				for (Uint16 x = 0U; x < width_in_tiles; ++x)
				{
					const std::size_t cell_index =
						static_cast<std::size_t>(y) * width_in_tiles + x;
					const Uint32 cell_offset = cells_offset +
						static_cast<Uint32>(cell_index * sizeof(Uint16));
					const Uint16 attributes = ReadBE16(rom, cell_offset);

					// Tile zero is the transparent/empty cell in this foreground
					// layout. Skipping it also crops the assembled frame to the
					// actual support instead of the full 320x224 plane.
					if ((attributes & kTileIndexMask) == 0U)
					{
						continue;
					}

					PieceInstance piece;
					piece.descriptor.rom_offset = cell_offset;
					piece.descriptor.width_in_tiles = 1U;
					piece.descriptor.height_in_tiles = 1U;
					piece.descriptor.x =
						(attributes & kHorizontalFlip) != 0U ? -8 : 0;
					piece.descriptor.y =
						(attributes & kVerticalFlip) != 0U ? -8 : 0;
					piece.descriptor.attributes = attributes;
					piece.effective_attributes = attributes;
					piece.position_x_pixels = static_cast<int>(x) * 8;
					piece.position_y_pixels = static_cast<int>(y) * 8;
					piece.writable = true;
					definition.pieces.emplace_back(std::move(piece));
				}
			}

			if (definition.pieces.empty())
			{
				error = "Title-screen bumper layout contains no visible tiles";
				return false;
			}
			output.emplace_back(std::move(definition));
			return true;
		}

		bool BuildFrameDefinitions(
			const std::vector<Uint8>& rom,
			std::vector<FrameDefinition>& output,
			std::string& error
		)
		{
			output.clear();
			for (std::size_t index = 0U; index < kTitleSonicObjectTables.size(); ++index)
			{
				if (!AppendObjectTableFrame(
					rom,
					kTitleSonicObjectTables[index],
					TitleScreenCategory::SONIC,
					"Sonic - frame " + std::to_string(index + 1U),
					"Sonic title-screen animation",
					index,
					output,
					error
				))
				{
					return false;
				}
			}
			if (!AppendObjectTableFrame(
				rom,
				kTitleAdditionalSonicObjectTable,
				TitleScreenCategory::SONIC,
				"Sonic - frame 8",
				"Additional Sonic title-screen animation composition",
				7U,
				output,
				error
			))
			{
				return false;
			}
			if (!AppendTileLayoutFrame(
				rom,
				kTitleBumperRingTileLayout,
				TitleScreenCategory::BUMPER_RING,
				"Bumper / Ring",
				"Pinball support on which Sonic bounces",
				100U,
				output,
				error
			))
			{
				return false;
			}
			if (!AppendObjectTableFrame(
				rom,
				kTitleLogoSonicObjectTable,
				TitleScreenCategory::LOGO_SONIC,
				"Logo Sonic",
				"SONIC title logo",
				200U,
				output,
				error
			))
			{
				return false;
			}
			if (!AppendObjectTableFrame(
				rom,
				kTitleLogoTheHedgehogObjectTable,
				TitleScreenCategory::LOGO_THE_HEDGEHOG,
				"Logo The Hedgehog",
				"THE HEDGEHOG title logo",
				300U,
				output,
				error
			))
			{
				return false;
			}
			if (!AppendObjectTableFrame(
				rom,
				kTitleLogoSpinballObjectTable,
				TitleScreenCategory::LOGO_SPINBALL,
				"Logo Spinball",
				"SPINBALL title logo",
				400U,
				output,
				error
			))
			{
				return false;
			}
			return true;
		}

		bool PieceFitsArt(const PieceInstance& piece, const std::vector<Uint8>& art)
		{
			const Uint16 absolute_tile = static_cast<Uint16>(
				piece.effective_attributes & kTileIndexMask
			);
			if (absolute_tile < kTitleBaseTile)
			{
				return false;
			}
			const std::size_t first_tile = absolute_tile - kTitleBaseTile;
			const std::size_t tile_count =
				static_cast<std::size_t>(piece.descriptor.width_in_tiles) *
				piece.descriptor.height_in_tiles;
			const std::size_t available_tiles = art.size() / kTileBytes;
			return first_tile <= available_tiles && tile_count <= available_tiles - first_tile;
		}

		std::size_t FilterPiecesForArt(FrameDefinition& definition, const std::vector<Uint8>& art)
		{
			const std::size_t before = definition.pieces.size();
			definition.pieces.erase(
				std::remove_if(
					definition.pieces.begin(),
					definition.pieces.end(),
					[&art](const PieceInstance& piece)
					{
						return !PieceFitsArt(piece, art);
					}
				),
				definition.pieces.end()
			);
			return before - definition.pieces.size();
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
			std::string& error
		)
		{
			if (!PieceFitsArt(piece, art))
			{
				error = "Object piece references tiles outside the title-screen art";
				return nullptr;
			}
			const int width = static_cast<int>(piece.descriptor.width_in_tiles) * 8;
			const int height = static_cast<int>(piece.descriptor.height_in_tiles) * 8;
			const Uint16 absolute_tile = static_cast<Uint16>(
				piece.effective_attributes & kTileIndexMask
			);
			const std::size_t first_tile = absolute_tile - kTitleBaseTile;

			auto sprite_tile = std::make_shared<SpriteTile>();
			sprite_tile->x_size = static_cast<Uint8>(width);
			sprite_tile->y_size = static_cast<Uint8>(height);
			sprite_tile->x_offset = static_cast<Sint16>(DisplayX(piece));
			sprite_tile->y_offset = static_cast<Sint16>(DisplayY(piece));
			sprite_tile->blit_settings.flip_horizontal =
				(piece.effective_attributes & kHorizontalFlip) != 0U;
			sprite_tile->blit_settings.flip_vertical =
				(piece.effective_attributes & kVerticalFlip) != 0U;
			sprite_tile->palette_line = static_cast<Uint8>(
				(piece.effective_attributes >> 13U) & 3U
			);
			sprite_tile->pixel_data.assign(
				static_cast<std::size_t>(width) * height,
				0U
			);

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
							const Uint8 palette_line = static_cast<Uint8>(
								(piece.effective_attributes >> 13U) & 3U
							);
							// Preserve the mapping attribute in the pixel index. Index zero
							// remains the universal transparent value; visible colours use
							// 1-15, 17-31, 33-47 or 49-63.
							const Uint8 combined_colour = colour == 0U
								? 0U
								: static_cast<Uint8>((palette_line * 16U) + colour);
							sprite_tile->pixel_data[
								static_cast<std::size_t>(destination_y) * width + destination_x
							] = combined_colour;
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
			if (definition.pieces.empty())
			{
				error = "Frame contains no title-screen art pieces";
				return nullptr;
			}
			const PixelBounds bounds = ComputeBounds(definition);
			if (bounds.Width() <= 0 || bounds.Height() <= 0 ||
				bounds.Width() > kMaximumFrameDimension ||
				bounds.Height() > kMaximumFrameDimension)
			{
				error = "Frame bounds are invalid or unexpectedly large";
				return nullptr;
			}

			auto sprite = std::make_shared<Sprite>();
			Uint32 total_vdp_tiles = 0U;
			Uint32 minimum_descriptor = std::numeric_limits<Uint32>::max();
			Uint32 maximum_descriptor = 0U;
			for (const PieceInstance& piece : definition.pieces)
			{
				std::shared_ptr<SpriteTile> tile = BuildSpriteTile(piece, art, error);
				if (!tile)
				{
					return nullptr;
				}
				sprite->sprite_tiles.emplace_back(std::move(tile));
				total_vdp_tiles += static_cast<Uint32>(piece.descriptor.width_in_tiles) *
					piece.descriptor.height_in_tiles;
				minimum_descriptor = std::min(minimum_descriptor, piece.descriptor.rom_offset);
				maximum_descriptor = std::max(maximum_descriptor, piece.descriptor.rom_offset + 8U);
			}
			if (sprite->sprite_tiles.empty() ||
				sprite->sprite_tiles.size() > std::numeric_limits<Uint16>::max() ||
				total_vdp_tiles > std::numeric_limits<Uint16>::max())
			{
				error = "Complete title-screen frame has an invalid tile count";
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
			if (sprite.sprite_tiles.empty()) return image;
			const auto bounds = sprite.GetBoundingBox();
			image.width = bounds.Width();
			image.height = bounds.Height();
			if (image.width <= 0 || image.height <= 0) return {};
			image.pixels.assign(static_cast<std::size_t>(image.width) * image.height, 0U);
			for (auto piece_it = sprite.sprite_tiles.rbegin();
				piece_it != sprite.sprite_tiles.rend(); ++piece_it)
			{
				const std::shared_ptr<SpriteTile>& piece = *piece_it;
				if (!piece) continue;
				for (int displayed_y = 0; displayed_y < piece->y_size; ++displayed_y)
				{
					for (int displayed_x = 0; displayed_x < piece->x_size; ++displayed_x)
					{
						const int source_x = piece->blit_settings.flip_horizontal
							? piece->x_size - 1 - displayed_x : displayed_x;
						const int source_y = piece->blit_settings.flip_vertical
							? piece->y_size - 1 - displayed_y : displayed_y;
						const Uint8 colour = static_cast<Uint8>(piece->pixel_data[
							static_cast<std::size_t>(source_y) * piece->x_size + source_x
						] & 0x3FU);
						if ((colour & 0x0FU) == 0U) continue;
						const int destination_x = piece->x_offset - bounds.min.x + displayed_x;
						const int destination_y = piece->y_offset - bounds.min.y + displayed_y;
						image.pixels[
							static_cast<std::size_t>(destination_y) * image.width + destination_x
						] = colour;
					}
				}
			}
			return image;
		}

		std::vector<Uint8> BuildPaletteLineMap(const Sprite& sprite)
		{
			std::vector<Uint8> palette_lines;
			if (sprite.sprite_tiles.empty()) return palette_lines;
			const BoundingBox bounds = sprite.GetBoundingBox();
			const int width = bounds.Width();
			const int height = bounds.Height();
			if (width <= 0 || height <= 0) return palette_lines;
			palette_lines.assign(static_cast<std::size_t>(width) * height, 0U);

			for (int image_y = 0; image_y < height; ++image_y)
			{
				for (int image_x = 0; image_x < width; ++image_x)
				{
					std::optional<Uint8> transparent_fallback;
					std::optional<Uint8> visible_owner;
					for (const std::shared_ptr<SpriteTile>& piece : sprite.sprite_tiles)
					{
						if (!piece) continue;
						const int displayed_x = image_x + bounds.min.x - piece->x_offset;
						const int displayed_y = image_y + bounds.min.y - piece->y_offset;
						if (displayed_x < 0 || displayed_x >= piece->x_size ||
							displayed_y < 0 || displayed_y >= piece->y_size)
						{
							continue;
						}

						const int source_x = piece->blit_settings.flip_horizontal
							? piece->x_size - 1 - displayed_x : displayed_x;
						const int source_y = piece->blit_settings.flip_vertical
							? piece->y_size - 1 - displayed_y : displayed_y;
						const Uint8 colour = static_cast<Uint8>(piece->pixel_data[
							static_cast<std::size_t>(source_y) * piece->x_size + source_x
						] & 0x3FU);
						if (!transparent_fallback.has_value())
						{
							transparent_fallback = piece->palette_line;
						}
						if ((colour & 0x0FU) != 0U)
						{
							visible_owner = piece->palette_line;
							break;
						}
					}

					palette_lines[static_cast<std::size_t>(image_y) * width + image_x] =
						visible_owner.value_or(transparent_fallback.value_or(0U));
				}
			}
			return palette_lines;
		}

		void UpdateMegaDriveChecksum(SpinballROM& rom)
		{
			if (rom.m_buffer.size() < 0x190U) return;
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
			if (!PieceFitsArt(piece, art))
			{
				error = "Object piece references tiles outside the title-screen art";
				return false;
			}
			const bool horizontal_flip = (piece.effective_attributes & kHorizontalFlip) != 0U;
			const bool vertical_flip = (piece.effective_attributes & kVerticalFlip) != 0U;
			const int unflipped_x = horizontal_flip
				? piece_width - 1 - displayed_local_x : displayed_local_x;
			const int unflipped_y = vertical_flip
				? piece_height - 1 - displayed_local_y : displayed_local_y;
			const Uint16 absolute_tile = static_cast<Uint16>(
				piece.effective_attributes & kTileIndexMask
			);
			const std::size_t first_tile = absolute_tile - kTitleBaseTile;
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
			if (indexed_pixels.size() < static_cast<std::size_t>(image_width) * image_height)
			{
				error = "PNG indexed-pixel buffer is smaller than its dimensions";
				return false;
			}
			const PixelBounds bounds = ComputeBounds(definition);
			if (image_width != bounds.Width() || image_height != bounds.Height())
			{
				std::ostringstream message;
				message << "PNG must be exactly " << bounds.Width() << 'x' << bounds.Height()
					<< " pixels for this assembled title frame (received "
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

			for (int image_y = 0; image_y < image_height; ++image_y)
			{
				for (int image_x = 0; image_x < image_width; ++image_x)
				{
					std::optional<Candidate> transparent_fallback;
					std::optional<Candidate> owner;
					for (const PieceInstance& piece : definition.pieces)
					{
						if (!piece.writable) continue;
						const int local_x = image_x + bounds.minimum_x - DisplayX(piece);
						const int local_y = image_y + bounds.minimum_y - DisplayY(piece);
						Uint8 current_colour = 0U;
						std::size_t byte_offset = 0U;
						bool high_nibble = false;
						if (!ReadPiecePixel(
							piece, art, local_x, local_y, current_colour,
							byte_offset, high_nibble, error
						))
						{
							if (!error.empty()) return false;
							continue;
						}
						const Candidate candidate{byte_offset, high_nibble, current_colour};
						if (!transparent_fallback.has_value()) transparent_fallback = candidate;
						if (current_colour != 0U)
						{
							owner = candidate;
							break;
						}
					}
					const std::optional<Candidate>& selected = owner.has_value()
						? owner : transparent_fallback;
					if (!selected.has_value()) continue;
					const Uint8 imported_colour = static_cast<Uint8>(indexed_pixels[
						static_cast<std::size_t>(image_y) * image_width + image_x
					] & 0x0FU);
					if (imported_colour == selected->current_colour) continue;
					const std::size_t source_key = selected->byte_offset * 2U +
						(selected->high_nibble ? 0U : 1U);
					const auto existing = changed_pixels.find(source_key);
					if (existing != changed_pixels.end())
					{
						if (existing->second.colour != imported_colour)
						{
							++conflicting_shared_pixels;
						}
						continue;
					}
					changed_pixels.emplace(source_key, PixelAssignment{
						selected->byte_offset, selected->high_nibble, imported_colour
					});
				}
			}
			for (const auto& [source_key, assignment] : changed_pixels)
			{
				(void)source_key;
				WritePackedPixel(art, assignment.byte_offset, assignment.high_nibble, assignment.colour);
			}
			changed_pixel_count = changed_pixels.size();
			return true;
		}
	}

	TitleScreenDecodeResult TitleScreenDecoder::Decode(const SpinballROM& rom)
	{
		TitleScreenDecodeResult result;
		if (!CanRead(rom.m_buffer, kTitleArtHeaderOffset, 2U) ||
			ReadBE16(rom.m_buffer, kTitleArtHeaderOffset) != 0xFFFFU)
		{
			result.error = "Title-screen Compressed2 marker FFFF is missing at 0x9D102";
			return result;
		}

		std::vector<Uint8> art;
		std::string error;
		if (!Compressed2Optimizer::Decode(
			rom.m_buffer,
			kTitleCompressedStreamOffset,
			art,
			error,
			nullptr,
			nullptr
		))
		{
			result.error = "Could not decompress title-screen art at 0x9D104: " + error;
			return result;
		}
		if (art.empty())
		{
			result.error = "Title-screen art decompressed to an empty tile payload";
			return result;
		}

		auto palette_set = std::make_shared<PaletteSet>();
		for (std::size_t line = 0U; line < palette_set->palette_lines.size(); ++line)
		{
			palette_set->palette_lines[line] = Palette::LoadFromROM(
				rom,
				kTitlePaletteDataOffset + static_cast<Uint32>(line * Palette::s_palette_size_on_rom)
			);
			if (!palette_set->palette_lines[line])
			{
				result.error = "Could not load the four title-screen palette lines";
				return result;
			}
		}
		result.palette_set = std::move(palette_set);

		std::vector<FrameDefinition> definitions;
		if (!BuildFrameDefinitions(rom.m_buffer, definitions, error))
		{
			result.error = error;
			return result;
		}

		std::vector<std::pair<TitleScreenCategory, RasterImage>> unique_rasters;
		for (FrameDefinition definition : definitions)
		{
			const std::size_t ignored_pieces = FilterPiecesForArt(definition, art);
			if (ignored_pieces != 0U)
			{
				std::ostringstream warning;
				warning << definition.name << ": ignored " << ignored_pieces
					<< " object piece(s) that use another graphics block";
				result.warnings.emplace_back(warning.str());
			}
			if (definition.pieces.empty()) continue;

			std::shared_ptr<const Sprite> sprite = BuildSprite(definition, art, error);
			if (!sprite)
			{
				result.warnings.emplace_back(definition.name + ": " + error);
				continue;
			}
			const RasterImage raster = RasteriseSprite(*sprite);
			const bool duplicate_in_category = std::any_of(
				unique_rasters.begin(),
				unique_rasters.end(),
				[&definition, &raster](
					const std::pair<TitleScreenCategory, RasterImage>& candidate
				)
				{
					return candidate.first == definition.category && candidate.second == raster;
				}
			);
			if (duplicate_in_category)
			{
				continue;
			}
			unique_rasters.emplace_back(definition.category, raster);
			TitleScreenFrame frame;
			frame.category = definition.category;
			frame.name = std::move(definition.name);
			frame.usage = std::move(definition.usage);
			frame.frame_id = definition.frame_id;
			frame.palette_line_map = BuildPaletteLineMap(*sprite);
			frame.sprite = std::move(sprite);
			result.frames.emplace_back(std::move(frame));
		}
		if (result.frames.empty())
		{
			result.error = "No complete title-screen frame could be assembled";
		}
		return result;
	}

	TitleScreenImportResult TitleScreenDecoder::ImportIndexedImage(
		SpinballROM& rom,
		const SpinballROM& reference_rom,
		const std::size_t frame_id,
		const std::vector<Uint8>& indexed_pixels,
		const int image_width,
		const int image_height
	)
	{
		TitleScreenImportResult result;
		if (!CanRead(rom.m_buffer, kTitleArtHeaderOffset, 2U) ||
			ReadBE16(rom.m_buffer, kTitleArtHeaderOffset) != 0xFFFFU)
		{
			result.message = "Compressed2 marker FFFF is missing at 0x9D102";
			return result;
		}
		if (!CanRead(reference_rom.m_buffer, kTitleArtHeaderOffset, 2U) ||
			ReadBE16(reference_rom.m_buffer, kTitleArtHeaderOffset) != 0xFFFFU)
		{
			result.message = "Reference ROM has no Compressed2 marker at 0x9D102";
			return result;
		}

		std::vector<Uint8> tile_art;
		std::size_t current_stream_size = 0U;
		std::string error;
		if (!Compressed2Optimizer::Decode(
			rom.m_buffer,
			kTitleCompressedStreamOffset,
			tile_art,
			error,
			&current_stream_size,
			nullptr
		))
		{
			result.message = "Could not decompress working title art: " + error;
			return result;
		}
		if (tile_art.empty())
		{
			result.message = "Working title art has no tile payload";
			return result;
		}

		std::vector<FrameDefinition> definitions;
		if (!BuildFrameDefinitions(rom.m_buffer, definitions, error))
		{
			result.message = error;
			return result;
		}
		auto definition_it = std::find_if(
			definitions.begin(), definitions.end(),
			[frame_id](const FrameDefinition& definition)
			{
				return definition.frame_id == frame_id;
			}
		);
		if (definition_it == definitions.end())
		{
			result.message = "Selected title-screen frame ID is not available";
			return result;
		}
		FrameDefinition definition = *definition_it;
		FilterPiecesForArt(definition, tile_art);
		if (definition.pieces.empty())
		{
			result.message = "Selected frame contains no piece from the title-screen art block";
			return result;
		}

		std::vector<Uint8> reference_tile_art;
		std::size_t capacity = 0U;
		if (!Compressed2Optimizer::Decode(
			reference_rom.m_buffer,
			kTitleCompressedStreamOffset,
			reference_tile_art,
			error,
			&capacity,
			nullptr
		))
		{
			result.message = "Could not determine exact reference capacity: " + error;
			return result;
		}
		if (kTitleCompressedStreamOffset > reference_rom.m_buffer.size() ||
			capacity > reference_rom.m_buffer.size() - kTitleCompressedStreamOffset)
		{
			result.message = "Reference title-art block extends outside the ROM";
			return result;
		}
		if (kTitleCompressedStreamOffset > rom.m_buffer.size() ||
			current_stream_size > rom.m_buffer.size() - kTitleCompressedStreamOffset)
		{
			result.message = "Working title-art block extends outside the ROM";
			return result;
		}
		const std::vector<Uint8> original_stream(
			rom.m_buffer.begin() + kTitleCompressedStreamOffset,
			rom.m_buffer.begin() + kTitleCompressedStreamOffset + current_stream_size
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
			result.message = "The PNG is identical to the current title frame; the ROM was not modified.";
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
			result.message = "Optimized Compressed2 validation produced different title art";
			return result;
		}
		if (compression.data.size() > capacity)
		{
			std::ostringstream message;
			message << "Import refused before writing the ROM. Modified title art needs "
				<< compression.data.size() << " bytes after optimized compression, but "
				<< "the original block only has " << capacity << " bytes ("
				<< compression.data.size() - capacity << " bytes too large; basic "
				<< "compression was " << compression.baseline_size << " bytes).";
			result.message = message.str();
			return result;
		}
		if (capacity > rom.m_buffer.size() - kTitleCompressedStreamOffset)
		{
			result.message = "Compressed title-art block extends outside the working ROM";
			return result;
		}

		std::copy(
			compression.data.begin(), compression.data.end(),
			rom.m_buffer.begin() + kTitleCompressedStreamOffset
		);
		std::fill(
			rom.m_buffer.begin() + kTitleCompressedStreamOffset + compression.data.size(),
			rom.m_buffer.begin() + kTitleCompressedStreamOffset + capacity,
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
		message << ". All title frames referencing these source tiles use the change.";
		result.message = message.str();
		return result;
	}
}
