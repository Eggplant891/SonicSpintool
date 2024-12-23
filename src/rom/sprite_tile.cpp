#include "rom/sprite_tile.h"

#include "render.h"
#include "rom/spinball_rom.h"

namespace spintool::rom
{
	void rom::SpriteTile::RenderToSurface(SDL_Surface* surface) const
	{
		Renderer::s_sdl_update_mutex.lock();
		SDL_LockSurface(surface);

		SDL_ClearSurface(surface, 0, 0, 0, 0);

		const BoundingBox& bounds{ x_offset, y_offset, x_offset + x_size, y_offset + y_size };

		if (bounds.Width() > 0 && bounds.Height() > 0)
		{
			BlitPixelDataToSurface(surface, bounds, pixel_data);
		}

		SDL_UnlockSurface(surface);
		Renderer::s_sdl_update_mutex.unlock();
	}

	void rom::SpriteTile::BlitPixelDataToSurface(SDL_Surface* surface, const BoundingBox& bounds, const std::vector<Uint32>& pixels_data) const
	{
		int x_off = (x_offset - bounds.min.x);
		int y_off = (y_offset - bounds.min.y);
		int x_max = x_off + x_size;
		int y_max = y_off + y_size;

		size_t target_pixel_index = (y_off * surface->pitch) + x_off;

		for (size_t i = 0; i < pixels_data.size() && i < surface->pitch * surface->h && i / x_max < y_max; ++i, target_pixel_index += 1)
		{
			if ((i % x_size) == 0)
			{
				target_pixel_index = (y_off * surface->pitch) + (surface->pitch * (i / x_size)) + x_off;
			}

			//*(Uint32*)(&((Uint8*)surface->pixels)[target_pixel_index]) = SDL_MapRGB(Renderer::s_pixel_format_details, NULL, static_cast<Uint8>((pixel_data[i] / 16.0f) * 255), 0, 0);
			((Uint8*)surface->pixels)[target_pixel_index] = pixels_data[i];
		}
	}

	const Uint8* SpriteTileHeader::LoadFromROM(const Uint8* rom_data_start, const size_t rom_data_offset)
	{
		const Uint8* current_byte = rom_data_start;

		x_offset = ((static_cast<Sint16>(*(current_byte)) << 8) | static_cast<Sint16>(*(current_byte + 1)));
		current_byte += 2;

		y_offset = (static_cast<Sint16>(*(current_byte)) << 8) | static_cast<Sint16>(*(current_byte + 1));
		current_byte += 2;

		y_size = *current_byte;
		++current_byte;

		x_size = *current_byte * 2;
		++current_byte;

		header_rom_data.SetROMData(rom_data_start, current_byte, rom_data_offset);

		return current_byte;
	}

	const Uint8* SpriteTileData::LoadFromROM(const SpriteTileHeader& header, const size_t rom_data_offset, const SpinballROM& src_rom)
	{
		const Uint8* rom_data_start = &src_rom.m_buffer[rom_data_offset];
		const Uint8* current_byte = rom_data_start;
		const size_t total_pixels = header.x_size * header.y_size;


		pixel_data.reserve(total_pixels);
		for (size_t i = 0; i < total_pixels && rom_data_offset + i < src_rom.m_buffer.size(); i += 2)
		{
			const Uint32 left_byte = (0xF0 & *current_byte) >> 4;
			const Uint32 right_byte = 0x0F & *current_byte;

			pixel_data.emplace_back(left_byte);
			pixel_data.emplace_back(right_byte);

			++current_byte;
		}

		tile_rom_data.SetROMData(rom_data_start, current_byte, rom_data_offset);

		return current_byte;
	}

}