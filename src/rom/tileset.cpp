#include "rom/tileset.h"
#include "rom/sprite.h"

namespace spintool::rom
{
	std::shared_ptr<const Sprite> TileSet::CreateSpriteFromTile(const size_t offset) const
	{
		if (offset >= raw_data.size() || offset + 64 >= raw_data.size())
		{
			return nullptr;
		}

		const Uint8* current_byte = &raw_data[offset];

		std::shared_ptr<rom::Sprite> new_sprite = std::make_shared<rom::Sprite>();

		new_sprite->rom_data.rom_offset = rom_data.rom_offset;
		const int num_tiles_to_wrangle = num_tiles;
		new_sprite->num_tiles = num_tiles_to_wrangle;

		int current_x_offset = 0;
		int current_y_offset = 0;

		for (int i = 0; i < num_tiles_to_wrangle; ++i)
		{
			std::shared_ptr<rom::SpriteTile> sprite_tile = new_sprite->sprite_tiles.emplace_back(std::make_shared<rom::SpriteTile>());
			sprite_tile->x_size = 8;
			sprite_tile->y_size = 8;

			if (i != 0)
			{
				if (i % 16 == 0)
				{
					current_x_offset = 0;
					current_y_offset += 9;

				}
				else
				{
					current_x_offset += 9;
				}
			}

			sprite_tile->x_offset = current_x_offset;
			sprite_tile->y_offset = current_y_offset;

			size_t total_pixels = sprite_tile->x_size * sprite_tile->y_size;
			for (size_t i = 0; i < total_pixels && sprite_tile->tile_rom_data.rom_offset + i < raw_data.size(); i += 2)
			{
				const Uint32 left_byte = (0xF0 & *current_byte) >> 4;
				const Uint32 right_byte = 0x0F & *current_byte;

				sprite_tile->pixel_data.emplace_back(left_byte);
				sprite_tile->pixel_data.emplace_back(right_byte);

				++current_byte;
			}
		}
		new_sprite->rom_data.SetROMData(&raw_data[offset], current_byte, offset);

		return new_sprite;
	}

}