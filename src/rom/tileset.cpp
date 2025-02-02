#include "rom/tileset.h"
#include "rom/sprite.h"
#include "rom/spinball_rom.h"
#include "rom/ssc_decompressor.h"
#include "rom/second_decompressor.h"

namespace spintool::rom
{
	TilesetEntry TileSet::LoadFromROM(const SpinballROM& src_rom, size_t rom_offset)
	{
		auto new_tileset = std::make_shared<rom::TileSet>();

		new_tileset->num_tiles = (static_cast<Sint16>(*(&src_rom.m_buffer[rom_offset])) << 8) | static_cast<Sint16>(*(&src_rom.m_buffer[rom_offset + 1]));
		new_tileset->uncompressed_data.clear();

		SSCDecompressionResult results = rom::SSCDecompressor::DecompressData(src_rom.m_buffer, rom_offset + 2, new_tileset->num_tiles * 64);
		new_tileset->uncompressed_size = results.uncompressed_data.size();
		new_tileset->compressed_size = results.rom_data.real_size;
		new_tileset->uncompressed_data = std::move(results.uncompressed_data);
		new_tileset->rom_data.SetROMData(results.rom_data.rom_offset - 2, results.rom_data.rom_offset_end);

		return { new_tileset, results };
	}

	TilesetEntry TileSet::LoadFromROMSecondCompression(const SpinballROM& src_rom, size_t rom_offset)
	{
		auto new_tileset = std::make_shared<rom::TileSet>();

		//new_tileset->num_tiles = (static_cast<Sint16>(*(&src_rom.m_buffer[rom_offset])) << 8) | static_cast<Sint16>(*(&src_rom.m_buffer[rom_offset + 1]));
		new_tileset->uncompressed_data.clear();

		SecondDecompressionResult results = rom::SecondDecompressor::DecompressData(src_rom.m_buffer, rom_offset);
		new_tileset->uncompressed_size = results.uncompressed_data.size();
		new_tileset->compressed_size = results.rom_data.real_size;
		new_tileset->uncompressed_data = std::move(results.uncompressed_data);
		new_tileset->rom_data.SetROMData(results.rom_data.rom_offset, results.rom_data.rom_offset_end);

		new_tileset->num_tiles = static_cast<Uint16>(new_tileset->uncompressed_data.size() / (16 * 16));

		return { new_tileset, results };
	}

	std::shared_ptr<rom::SpriteTile> TileSet::CreateSpriteTileFromTile(const size_t tile_index) const
	{
		const size_t relative_offset = tile_index * s_tile_total_bytes;

		if (uncompressed_data.empty() || relative_offset >= uncompressed_data.size())
		{
			return nullptr;
		}

		std::shared_ptr<rom::SpriteTile> sprite_tile = std::make_shared<rom::SpriteTile>();
		sprite_tile->x_size = s_tile_width;
		sprite_tile->y_size = s_tile_height;

		const int num_tiles_per_row = 16;
		const int tile_spacing = 0;
		size_t current_x_offset = 0;
		size_t current_y_offset = 0;


		if (tile_index != 0)
		{
			const size_t x_offset = tile_index % num_tiles_per_row;
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

		sprite_tile->tile_rom_data.SetROMData(rom_data.rom_offset + relative_offset, rom_data.rom_offset + relative_offset + (current_byte - tile_start_byte));

		return sprite_tile;
	}

	std::shared_ptr<const Sprite> TileSet::CreateSpriteFromTile(const size_t relative_offset) const
	{
		if (relative_offset >= uncompressed_data.size() || relative_offset + s_tile_total_bytes >= uncompressed_data.size())
		{
			return nullptr;
		}

		std::shared_ptr<rom::Sprite> new_sprite = std::make_shared<rom::Sprite>();

		const Uint8* tileset_start_byte = &uncompressed_data[relative_offset];
		const Uint8* current_byte = tileset_start_byte;
		const size_t num_tiles_to_wrangle = num_tiles <= (uncompressed_data.size() / s_tile_total_pixels) ? num_tiles : (uncompressed_data.size() / s_tile_total_pixels);

		new_sprite->rom_data.rom_offset = rom_data.rom_offset;
		new_sprite->num_tiles = static_cast<Uint16>(num_tiles_to_wrangle);

		for (int i = 0; i < num_tiles_to_wrangle; ++i)
		{
			new_sprite->sprite_tiles.emplace_back(TileSet::CreateSpriteTileFromTile(i));
			current_byte += new_sprite->sprite_tiles.back()->tile_rom_data.real_size;
		}
		new_sprite->rom_data.SetROMData(rom_data.rom_offset + relative_offset, rom_data.rom_offset + (current_byte - &uncompressed_data[relative_offset]));

		return new_sprite;
	}
}