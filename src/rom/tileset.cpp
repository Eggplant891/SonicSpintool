#include "rom/tileset.h"
#include "rom/sprite.h"

namespace spintool::rom
{
	std::shared_ptr<const Sprite> TileSet::CreateSpriteFromTile(const size_t offset) const
	{
		if (offset >= uncompressed_data.size() || offset + s_tile_total_bytes >= uncompressed_data.size())
		{
			return nullptr;
		}

		std::shared_ptr<rom::Sprite> new_sprite = std::make_shared<rom::Sprite>();

		const Uint8* tileset_start_byte = &uncompressed_data[offset];
		const Uint8* current_byte = tileset_start_byte;
		const int num_tiles_to_wrangle = num_tiles;
		const int num_tiles_per_row = 16;
		const int tile_spacing = 0;

		new_sprite->rom_data.rom_offset = rom_data.rom_offset;
		new_sprite->num_tiles = num_tiles_to_wrangle;

		int current_x_offset = 0;
		int current_y_offset = 0;

		for (int i = 0; i < num_tiles_to_wrangle; ++i)
		{
			std::shared_ptr<rom::SpriteTile> sprite_tile = new_sprite->sprite_tiles.emplace_back(std::make_shared<rom::SpriteTile>());
			sprite_tile->x_size = s_tile_width;
			sprite_tile->y_size = s_tile_height;

			if (i != 0)
			{
				if (i % num_tiles_per_row == 0)
				{
					current_x_offset = 0;
					current_y_offset += s_tile_height + tile_spacing;

				}
				else
				{
					current_x_offset += s_tile_width + tile_spacing;
				}
			}

			sprite_tile->x_offset = current_x_offset;
			sprite_tile->y_offset = current_y_offset;

			const Uint8* tile_start_byte = current_byte;
			const size_t total_pixels = sprite_tile->x_size * sprite_tile->y_size;
			for (size_t i = 0; i < total_pixels && sprite_tile->tile_rom_data.rom_offset + i < uncompressed_data.size(); i += 2)
			{
				const Uint32 left_byte = (0xF0 & *current_byte) >> 4;
				const Uint32 right_byte = 0x0F & *current_byte;

				sprite_tile->pixel_data.emplace_back(left_byte);
				sprite_tile->pixel_data.emplace_back(right_byte);

				++current_byte;
			}

			sprite_tile->tile_rom_data.SetROMData(rom_data.rom_offset + offset + (tile_start_byte - tileset_start_byte), rom_data.rom_offset + offset + (current_byte - tileset_start_byte));
		}
		new_sprite->rom_data.SetROMData(rom_data.rom_offset + offset, rom_data.rom_offset + (current_byte - &uncompressed_data[offset]));

		return new_sprite;
	}

}