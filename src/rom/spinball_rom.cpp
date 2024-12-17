#include "rom/spinball_rom.h"

#include "render.h"
#include "types/sdl_handle_defs.h"
#include "rom/sprite.h"

#include <fstream>
#include "SDL3/SDL_stdinc.h"
#include "SDL3/SDL_surface.h"
#include "rom/palette.h"

namespace spintool
{
	Point rom::Sprite::GetOriginOffsetFromMinBounds() const
	{
		BoundingBox bounds = GetBoundingBox();
		return { -bounds.min.x, -bounds.min.y };
	}

	void rom::SpinballROM::LoadTileData(size_t rom_offset)
	{
		m_toxic_caves_bg_data.rom_offset = rom_offset;
		unsigned char* rom_data_root = &m_buffer[rom_offset];
		unsigned char* compressed_data_root = &m_buffer[rom_offset+2];

		m_toxic_caves_bg_data.num_tiles = (static_cast<Sint16>(*(rom_data_root)) << 8) | static_cast<Sint16>(*(rom_data_root + 1));

		std::vector<unsigned char> working_data; working_data.resize( m_toxic_caves_bg_data.num_tiles * 64 );

		while (true)
		{
			unsigned char fragment_header = *compressed_data_root;
			compressed_data_root = compressed_data_root + 1;
			unsigned char* next_byte = nullptr;

			int fragments_remaining = 7;
			while (fragments_remaining >= 0)
			{
				const bool is_raw_data = (fragment_header & 1) == 1;
				fragment_header = fragment_header >> 1;
				if (is_raw_data == false)
				{
					next_byte = compressed_data_root + 1;

					Uint16 tile_index_offset = (static_cast<Uint16>((*next_byte) & 0xF0) << 4) | static_cast<Uint16>(*compressed_data_root);
					if (tile_index_offset == 0)
					{
						return;
					}

					for (int num_copies = (*next_byte & 0xF) + 1; num_copies >= 0; --num_copies)
					{
						unsigned char value_to_write = working_data.at(tile_index_offset);
						m_toxic_caves_bg_data.data.emplace_back(value_to_write);
						working_data[m_toxic_caves_bg_data.data.size() & 0xFFF] = value_to_write;
						tile_index_offset = (tile_index_offset + 1) & 0xFFF;
					}
				}
				else
				{
					m_toxic_caves_bg_data.data.emplace_back(*compressed_data_root);
					working_data[m_toxic_caves_bg_data.data.size() & 0xFFF] = *compressed_data_root;
					next_byte = compressed_data_root;
				}
				compressed_data_root = next_byte + 1;
				--fragments_remaining;
			}
		}
	}

	bool rom::SpinballROM::LoadROMFromPath(const std::filesystem::path& path)
	{
		std::ifstream input = std::ifstream{ path, std::ios::binary };
		m_filepath = path;
		m_buffer = std::vector<unsigned char>{ std::istreambuf_iterator<char>(input), {} };
		return m_buffer.empty() == false;
	}

	void rom::SpinballROM::SaveROM()
	{
		std::ofstream output = std::ofstream{ m_filepath, std::ios::binary };
		output.write(reinterpret_cast<const char*>(m_buffer.data()), m_buffer.size());
	}

	size_t rom::SpinballROM::GetOffsetForNextSprite(const rom::Sprite& current_sprite) const
	{
		return current_sprite.rom_offset + current_sprite.GetSizeOf();
	}

	std::shared_ptr<const rom::Sprite> rom::SpinballROM::LoadSprite(size_t offset, bool packed_data_mode, bool try_to_read_missed_data)
	{
		if (offset + 4 < offset) // overflow detection
		{
			return nullptr;
		}

		unsigned char* current_byte = &m_buffer[offset];
		//unsigned char* current_byte = &buffer[0x1CE0];

		std::shared_ptr<rom::Sprite> new_sprite = std::make_shared<rom::Sprite>();

		new_sprite->rom_offset = current_byte - m_buffer.data();

		new_sprite->num_tiles = (static_cast<Sint16>(*(current_byte)) << 8) | static_cast<Sint16>(*(current_byte + 1));
		current_byte += 2;

		if (new_sprite->num_tiles > 0x20)
		{
			return nullptr;
		}

		new_sprite->num_vdp_tiles = (static_cast<Sint16>(*(current_byte)) << 8) | static_cast<Sint16>(*(current_byte + 1));
		current_byte += 2;

		new_sprite->sprite_tiles.resize(new_sprite->num_tiles);

		for (std::shared_ptr<rom::SpriteTile>& sprite_tile : new_sprite->sprite_tiles)
		{
			sprite_tile = std::make_shared<rom::SpriteTile>();

			sprite_tile->x_offset = ((static_cast<Sint16>(*(current_byte)) << 8) | static_cast<Sint16>(*(current_byte + 1)));
			current_byte += 2;

			sprite_tile->y_offset = (static_cast<Sint16>(*(current_byte)) << 8) | static_cast<Sint16>(*(current_byte + 1));
			current_byte += 2;

			sprite_tile->y_size = *current_byte;
			++current_byte;

			sprite_tile->x_size = *current_byte * 2;
			++current_byte;
		}

		if (new_sprite->GetBoundingBox().Width() > 512 || new_sprite->GetBoundingBox().Height() > 512)
		{
			return nullptr;
		}

		constexpr int max_tiles = 64;

		if (new_sprite->num_vdp_tiles > max_tiles)
		{
			return nullptr;
		}

		for (std::shared_ptr<rom::SpriteTile>& sprite_tile : new_sprite->sprite_tiles)
		{
			const size_t total_pixels = sprite_tile->x_size * sprite_tile->y_size;
			if (total_pixels != 0)
			{
				sprite_tile->rom_offset = current_byte - m_buffer.data();
				if (sprite_tile->rom_offset + ((sprite_tile->x_size / 2) * sprite_tile->y_size) > m_buffer.size())
				{
					continue;
				}

				sprite_tile->pixel_data.reserve(total_pixels);
				for (size_t i = 0; i < total_pixels && sprite_tile->rom_offset + i < m_buffer.size(); i+=2)
				{
					if (packed_data_mode)
					{
						const Uint32 left_byte = (0xF0 & *current_byte) >> 4;
						const Uint32 right_byte = 0x0F & *current_byte;

						sprite_tile->pixel_data.emplace_back(left_byte);
						sprite_tile->pixel_data.emplace_back(right_byte);

						++current_byte;
					}
					else
					{
						sprite_tile->pixel_data.emplace_back(*current_byte);
						++current_byte;
					}
				}

				sprite_tile->rom_offset_end = current_byte - m_buffer.data();

			}
		}

		new_sprite->real_size = current_byte - &m_buffer[offset];

		if (new_sprite->num_tiles == 0 || new_sprite->GetBoundingBox().Width() == 0 || new_sprite->GetBoundingBox().Height() == 0)
		{
			return nullptr;
		}

		return std::move(new_sprite);
	}

	std::shared_ptr<const spintool::rom::Sprite> rom::SpinballROM::LoadLevelTile(const std::vector<unsigned char>& data_source, const size_t offset)
	{
		if (offset >= data_source.size() || offset + 64 >= data_source.size())
		{
			return nullptr;
		}

		const unsigned char* current_byte = &data_source[offset];

		std::shared_ptr<rom::Sprite> new_sprite = std::make_shared<rom::Sprite>();

		new_sprite->rom_offset = m_toxic_caves_bg_data.rom_offset;
		constexpr int num_tiles_to_wrangle = 1;
		new_sprite->num_tiles = num_tiles_to_wrangle;

		for (int i = 0; i < num_tiles_to_wrangle; ++i)
		{
			std::shared_ptr<rom::SpriteTile> sprite_tile = new_sprite->sprite_tiles.emplace_back(std::make_shared<rom::SpriteTile>());
			sprite_tile->x_size = 8;
			sprite_tile->y_size = 8;

			if (i == 1)
			{
				sprite_tile->y_offset = 8;
			}
			else if (i == 2)
			{
				sprite_tile->x_offset = 8;
				sprite_tile->y_offset = 8;
			}
			else if (i == 3)
			{
				sprite_tile->x_offset = 8;
			}
			size_t total_pixels = sprite_tile->x_size * sprite_tile->y_size;

			for (size_t i = 0; i < total_pixels && sprite_tile->rom_offset + i < data_source.size(); i += 2)
			{
				const Uint32 left_byte = (0xF0 & *current_byte) >> 4;
				const Uint32 right_byte = 0x0F & *current_byte;

				sprite_tile->pixel_data.emplace_back(left_byte);
				sprite_tile->pixel_data.emplace_back(right_byte);

				++current_byte;
			}
		}

		new_sprite->rom_offset = m_toxic_caves_bg_data.rom_offset;
		new_sprite->real_size = current_byte - &data_source[offset];
		new_sprite->rom_offset_end = m_toxic_caves_bg_data.rom_offset + new_sprite->real_size;
		return new_sprite;
	}

	std::vector<spintool::rom::Palette> rom::SpinballROM::LoadPalettes(size_t num_palettes)
	{
		constexpr size_t offset = 0xDFC;
		unsigned char* current_byte = &m_buffer[offset];
		std::vector<spintool::rom::Palette> results;

		for (size_t p = 0; p < num_palettes; ++p)
		{
			rom::Palette palette;
			palette.offset = current_byte - m_buffer.data();

			for (size_t i = 0; i < 16; ++i)
			{
				const Uint8 first_byte = *current_byte;
				++current_byte;
				const Uint8 second_byte = *current_byte;
				++current_byte;
				rom::Swatch swatch;
				swatch.packed_value = static_cast<Uint16>((static_cast<Uint16>(first_byte) << 8) | second_byte);

				palette.palette_swatches[i] = swatch;
			}
			results.emplace_back(palette);
		}

		return results;
	}

	void rom::SpinballROM::RenderToSurface(SDL_Surface* surface, size_t offset, Point dimensions) const
	{
		Renderer::s_sdl_update_mutex.lock();
		SDL_LockSurface(surface);

		SDL_ClearSurface(surface, 0, 0, 0, 255);

		const BoundingBox bounds{ 0,0, dimensions.x, dimensions.y };

		const unsigned char* current_byte = &m_buffer[offset];
		std::vector<Uint32> pixels_data;
		for (int i = 0; i < dimensions.x * dimensions.y * 2; ++i)
		{
			Uint16 colour_data = (static_cast<Uint16>(*(current_byte)) << 8) | static_cast<Uint16>(*(current_byte + 1));
			pixels_data.emplace_back(colour_data);
			current_byte += 2;
		}

		BlitRawPixelDataToSurface(surface, bounds, pixels_data);

		SDL_UnlockSurface(surface);
		Renderer::s_sdl_update_mutex.unlock();
	}

	void rom::SpinballROM::BlitRawPixelDataToSurface(SDL_Surface* surface, const BoundingBox& bounds, const std::vector<Uint32>& pixels_data) const
	{
		int x_off = 0;
		int y_off = 0;
		int x_max = bounds.Width();
		int y_max = bounds.Height();
		;
		size_t target_pixel_index = (y_off * surface->pitch) + x_off;

		for (size_t i = 0; i < pixels_data.size() && i < surface->pitch * surface->h && i / x_max < y_max; ++i, target_pixel_index += 1)
		{
			if ((i % bounds.Width()) == 0)
			{
				target_pixel_index = (y_off * surface->pitch) + (surface->pitch * (i / bounds.Width())) + x_off;
			}
			rom::Swatch pixel_as_swatch = { static_cast<Uint16>(pixels_data[i]) };
			//((Uint32*)surface->pixels)[target_pixel_index] = SDL_MapRGB(SDL_GetPixelFormatDetails(surface->format), NULL, static_cast<Uint8>(pixel_as_swatch.GetUnpacked().r / 15.0f) * 255, static_cast<Uint8>(pixel_as_swatch.GetUnpacked().g / 15.0f) * 255, static_cast<Uint8>(pixel_as_swatch.GetUnpacked().b / 15.0f) * 255);
			((Uint8*)surface->pixels)[target_pixel_index] = pixels_data[i];
		}
	}
}