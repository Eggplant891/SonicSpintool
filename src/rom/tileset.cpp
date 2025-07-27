#include "rom/tileset.h"

#include "rom/sprite.h"
#include "rom/spinball_rom.h"
#include "rom/ssc_decompressor.h"
#include "rom/lzss_decompressor.h"
#include "rom/tile.h"
#include "rom/ssc_compressor.h"

namespace spintool::rom
{
	TilesetEntry TileSet::LoadFromROM(const SpinballROM& src_rom, Uint32 rom_offset, CompressionAlgorithm compression_algorithm)
	{
		switch (compression_algorithm)
		{
			case CompressionAlgorithm::SSC:
				return LoadFromROM_SSCCompression(src_rom, rom_offset);

			case CompressionAlgorithm::LZSS:
				return LoadFromROM_LZSSCompression(src_rom, rom_offset);

			case CompressionAlgorithm::NONE:
			default:
				return TilesetEntry{};
		}
	}

	Ptr32 TileSet::SaveToROM_SSCCompression(SpinballROM& src_rom, Uint32 rom_offset) const
	{
		Ptr32 current_offset = rom_offset;
		current_offset = src_rom.WriteUint16(current_offset, num_tiles);
		const SSCCompressionResult compressed_data = rom::SSCCompressor::CompressData(uncompressed_data, 0, num_tiles * 64);
		for (size_t i = 0; i < compressed_data.size(); ++i)
		{
			current_offset = src_rom.WriteUint8(current_offset, compressed_data.at(i));
		}

		return current_offset;
	}

	TilesetEntry TileSet::LoadFromROM_SSCCompression(const SpinballROM& src_rom, Uint32 rom_offset)
	{
		constexpr Uint32 uncompressed_tile_size = TileSet::s_tile_total_bytes;
		auto new_tileset = std::make_shared<rom::TileSet>();

		new_tileset->num_tiles = (static_cast<Sint16>(*(&src_rom.m_buffer[rom_offset])) << 8) | static_cast<Sint16>(*(&src_rom.m_buffer[rom_offset + 1]));
		new_tileset->uncompressed_data.clear();

		SSCDecompressionResult results = rom::SSCDecompressor::DecompressData(src_rom.m_buffer, rom_offset + 2, new_tileset->num_tiles * 64);

		new_tileset->uncompressed_size = static_cast<Uint32>(results.uncompressed_data.size());
		new_tileset->compressed_size = results.rom_data.real_size;
		new_tileset->uncompressed_data = std::move(results.uncompressed_data);

		for (size_t tile_index = 0; tile_index < new_tileset->num_tiles; ++tile_index)
		{
			const size_t relative_offset = (tile_index * s_tile_total_bytes);
			if (relative_offset >= new_tileset->uncompressed_data.size())
			{
				break;
			}

			rom::Tile new_tile;
			
			const Uint8* tile_start_byte = &new_tileset->uncompressed_data[relative_offset];
			const Uint8* current_byte = tile_start_byte;
			const size_t total_pixels = TileSet::s_tile_total_bytes;

			for (size_t i = 0; i < total_pixels && relative_offset + i < new_tileset->uncompressed_data.size(); i += 1)
			{
				const Uint32 left_byte = (0xF0 & *current_byte) >> 4;
				const Uint32 right_byte = 0x0F & *current_byte;

				new_tile.pixel_data.emplace_back(left_byte);
				new_tile.pixel_data.emplace_back(right_byte);

				++current_byte;
			}

			for (Uint8 x = 0; x < 4 && new_tile.is_x_symmetrical; ++x)
			{
				for (Uint8 y = 0; y < 8 && new_tile.is_x_symmetrical; ++y)
				{
					const Uint32 flip_lhs_index = (y * 8) + x;
					const Uint32 flip_rhs_index = (y * 8) + (7 - x);
					if (flip_lhs_index < new_tile.pixel_data.size() && flip_rhs_index < new_tile.pixel_data.size() && new_tile.pixel_data[flip_lhs_index] != new_tile.pixel_data[flip_rhs_index])
					{
						new_tile.is_x_symmetrical = false;
					}
				}
			}

			for (Uint8 x = 0; x < 8 && new_tile.is_y_symmetrical; ++x)
			{
				for (Uint8 y = 0; y < 4 && new_tile.is_y_symmetrical; ++y)
				{
					const Uint32 flip_lhs_index = (y * 8) + x;
					const Uint32 flip_rhs_index = ((7 - y) * 8) + x;
					if (flip_lhs_index < new_tile.pixel_data.size() && flip_rhs_index < new_tile.pixel_data.size() &&  new_tile.pixel_data[flip_lhs_index] != new_tile.pixel_data[flip_rhs_index])
					{
						new_tile.is_y_symmetrical = false;
					}
				}
			}

			new_tileset->tiles.emplace_back(std::move(new_tile));
		}

		new_tileset->rom_data.SetROMData(results.rom_data.rom_offset - 2, results.rom_data.rom_offset_end);

		return { new_tileset, results };
	}

	TilesetEntry TileSet::LoadFromROM_LZSSCompression(const SpinballROM& src_rom, Uint32 rom_offset)
	{
		auto new_tileset = std::make_shared<rom::TileSet>();

		//new_tileset->num_tiles = (static_cast<Sint16>(*(&src_rom.m_buffer[rom_offset])) << 8) | static_cast<Sint16>(*(&src_rom.m_buffer[rom_offset + 1]));
		new_tileset->uncompressed_data.clear();

		LZSSDecompressionResult results = rom::LZSSDecompressor::DecompressData(src_rom.m_buffer, rom_offset);
		LZSSDecompressionResult results2 = rom::LZSSDecompressor::DecompressDataRefactored(src_rom.m_buffer, rom_offset);

		if (results != results2)
		{
			results.error_msg = "Decompression Results Mismatch";
		}

		new_tileset->uncompressed_size = static_cast<Uint32>(results.uncompressed_data.size());
		new_tileset->compressed_size = results.rom_data.real_size;
		new_tileset->uncompressed_data = std::move(results.uncompressed_data);
		// Seems to be necessary to remove the first 2 bytes to make it renderable. Possible these specify dimensions or other data.
		new_tileset->uncompressed_data.erase(std::begin(new_tileset->uncompressed_data), std::begin(new_tileset->uncompressed_data) + 2);
		new_tileset->num_tiles = static_cast<Uint16>(new_tileset->uncompressed_data.size()) / TileSet::s_tile_total_bytes;

		for (size_t tile_index = 0; tile_index < new_tileset->num_tiles; ++tile_index)
		{
			const size_t relative_offset = (tile_index * s_tile_total_bytes);
			rom::Tile new_tile;

			const Uint8* tile_start_byte = &new_tileset->uncompressed_data[relative_offset];
			const Uint8* current_byte = tile_start_byte;
			const size_t total_pixels = TileSet::s_tile_total_bytes;

			for (size_t i = 0; i < total_pixels && relative_offset + i < new_tileset->uncompressed_data.size(); i += 1)
			{
				const Uint32 left_byte = (0xF0 & *current_byte) >> 4;
				const Uint32 right_byte = 0x0F & *current_byte;

				new_tile.pixel_data.emplace_back(left_byte);
				new_tile.pixel_data.emplace_back(right_byte);

				++current_byte;
			}

			new_tileset->tiles.emplace_back(std::move(new_tile));
		}

		new_tileset->rom_data.SetROMData(results.rom_data.rom_offset, results.rom_data.rom_offset_end);


		return { new_tileset, results };
	}

	std::shared_ptr<rom::SpriteTile> TileSet::CreateSpriteTileFromTile(const Uint32 tile_index) const
	{
		const Uint32 relative_offset = tile_index * s_tile_total_bytes;

		if (uncompressed_data.empty() || relative_offset >= uncompressed_data.size())
		{
			return nullptr;
		}

		std::shared_ptr<rom::SpriteTile> sprite_tile = std::make_shared<rom::SpriteTile>();
		sprite_tile->x_size = s_tile_width;
		sprite_tile->y_size = s_tile_height;

		const int num_tiles_per_row = 16;
		const int tile_spacing = 0;
		Uint32 current_x_offset = 0;
		Uint32 current_y_offset = 0;


		if (tile_index != 0)
		{
			const Uint32 x_offset = tile_index % num_tiles_per_row;
			if (x_offset == 0)
			{
				current_x_offset = 0;
				current_y_offset = (s_tile_height + tile_spacing) * (tile_index / num_tiles_per_row);

			}
			else
			{
				current_x_offset = (s_tile_width * x_offset) + tile_spacing;
				current_y_offset = (s_tile_height + tile_spacing) * ((tile_index- x_offset) / num_tiles_per_row);
			}
		}

		sprite_tile->x_offset = static_cast<Sint16>(current_x_offset);
		sprite_tile->y_offset = static_cast<Sint16>(current_y_offset);

		const size_t total_pixels = sprite_tile->x_size * sprite_tile->y_size;

		const Uint8* tile_start_byte = &uncompressed_data[relative_offset];
		const Uint8* current_byte = tile_start_byte;

		for (size_t i = 0; i < total_pixels && sprite_tile->tile_rom_data.rom_offset + i < uncompressed_data.size(); i += 2)
		{
			const Uint32 left_byte = (0xF0 & *current_byte) >> 4;
			const Uint32 right_byte = 0x0F & *current_byte;

			sprite_tile->pixel_data.emplace_back(left_byte);
			sprite_tile->pixel_data.emplace_back(right_byte);

			++current_byte;
		}

		sprite_tile->tile_rom_data.SetROMData(rom_data.rom_offset + relative_offset, rom_data.rom_offset + relative_offset + static_cast<Uint32>(current_byte - tile_start_byte));

		return sprite_tile;
	}

	std::shared_ptr<const Sprite> TileSet::CreateSpriteFromTile(const Uint32 relative_offset) const
	{
		if (relative_offset >= uncompressed_data.size() || relative_offset + s_tile_total_bytes >= uncompressed_data.size())
		{
			return nullptr;
		}

		std::shared_ptr<rom::Sprite> new_sprite = std::make_shared<rom::Sprite>();

		const Uint8* tileset_start_byte = &uncompressed_data[relative_offset];
		const Uint8* current_byte = tileset_start_byte;
		const Uint32 num_tiles_to_wrangle = num_tiles <= (static_cast<Uint32>(uncompressed_data.size()) / s_tile_total_pixels) ? num_tiles : (static_cast<Uint32>(uncompressed_data.size()) / s_tile_total_pixels);

		new_sprite->rom_data.rom_offset = rom_data.rom_offset;
		new_sprite->num_tiles = static_cast<Uint16>(num_tiles_to_wrangle);

		for (Uint32 i = 0; i < num_tiles_to_wrangle; ++i)
		{
			new_sprite->sprite_tiles.emplace_back(TileSet::CreateSpriteTileFromTile(i));
			current_byte += new_sprite->sprite_tiles.back()->tile_rom_data.real_size;
		}
		new_sprite->rom_data.SetROMData(rom_data.rom_offset + relative_offset, rom_data.rom_offset + static_cast<Uint32>(current_byte - &uncompressed_data[relative_offset]));

		return new_sprite;
	}
}