#include "rom/spinball_rom.h"

#include "render.h"
#include "types/sdl_handle_defs.h"
#include "rom/sprite.h"

#include <fstream>
#include "SDL3/SDL_stdinc.h"
#include "SDL3/SDL_surface.h"
#include "rom/palette.h"
#include "rom/ssc_decompressor.h"

namespace spintool
{
	Point rom::Sprite::GetOriginOffsetFromMinBounds() const
	{
		BoundingBox bounds = GetBoundingBox();
		return { -bounds.min.x, -bounds.min.y };
	}

	std::shared_ptr<const rom::TileSet> rom::SpinballROM::LoadTileData(size_t rom_offset)
	{
		auto new_tileset = std::make_shared<rom::TileSet>();
		
		new_tileset->num_tiles = (static_cast<Sint16>(*(&m_buffer[rom_offset])) << 8) | static_cast<Sint16>(*(&m_buffer[rom_offset+1]));
		new_tileset->raw_data.clear();
		SSCDecompressionResult results = rom::SSCDecompressor::DecompressData(&m_buffer[rom_offset+2], new_tileset->num_tiles * 64);
		new_tileset->uncompressed_size = results.uncompressed_data.size();
		new_tileset->raw_data = std::move(results.uncompressed_data);

		new_tileset->rom_data.SetROMData(&m_buffer[rom_offset+2], &m_buffer[rom_offset+2] + new_tileset->raw_data.size(), rom_offset);

		return new_tileset;
	}

	bool rom::SpinballROM::LoadROMFromPath(const std::filesystem::path& path)
	{
		std::ifstream input = std::ifstream{ path, std::ios::binary };
		m_filepath = path;
		m_buffer = std::vector<unsigned char>{ std::istreambuf_iterator<char>(input), {} };
		if (m_buffer.empty() == false)
		{
			LoadTileData(0x9bcd0);
		}
		return m_buffer.empty() == false;
	}

	void rom::SpinballROM::SaveROM()
	{
		std::ofstream output = std::ofstream{ m_filepath, std::ios::binary };
		output.write(reinterpret_cast<const char*>(m_buffer.data()), m_buffer.size());
	}

	size_t rom::SpinballROM::GetOffsetForNextSprite(const rom::Sprite& current_sprite) const
	{
		return current_sprite.rom_data.rom_offset + current_sprite.GetSizeOf();
	}

	std::shared_ptr<const rom::Sprite> rom::SpinballROM::LoadSprite(size_t offset)
	{
		if (offset + 4 < offset) // overflow detection
		{
			return nullptr;
		}

		size_t next_byte_offset = offset;

		std::shared_ptr<rom::Sprite> new_sprite = std::make_shared<rom::Sprite>();
		next_byte_offset = new_sprite->LoadFromROM(next_byte_offset, *this);

		if (next_byte_offset == offset || new_sprite->num_tiles == 0 || new_sprite->GetBoundingBox().Width() == 0 || new_sprite->GetBoundingBox().Height() == 0)
		{
			return nullptr;
		}

		return std::move(new_sprite);
	}

	std::vector<spintool::rom::Palette> rom::SpinballROM::LoadPalettes(size_t num_palettes)
	{
		constexpr size_t offset = 0xDFC;
		Uint8* current_byte = &m_buffer[offset];
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

	void rom::SpinballROM::RenderToSurface(SDL_Surface* surface, size_t offset, Point dimensions, const rom::Palette& palette) const
	{
		Renderer::s_sdl_update_mutex.lock();
		SDL_LockSurface(surface);

		SDL_ClearSurface(surface, 0, 0, 0, 255);

		const BoundingBox bounds{ 0,0, dimensions.x, dimensions.y };

		if (offset >= m_buffer.size())
		{
			return;
		}

		const SDL_PixelFormatDetails* pixel_format = SDL_GetPixelFormatDetails(surface->format);

		const Uint8* current_byte = &m_buffer[offset];
		std::vector<Uint32> pixels_data;
		for (int i = 0; i < dimensions.x * dimensions.y; i += 2)
		{
			const Uint32 left_byte = (0xF0 & *current_byte) >> 4;
			const Uint32 right_byte = 0x0F & *current_byte;

			const Colour left_byte_col = palette.palette_swatches.at(left_byte).GetUnpacked();
			const Colour right_byte_col = palette.palette_swatches.at(right_byte).GetUnpacked();

			pixels_data.emplace_back(SDL_MapRGB(pixel_format, nullptr, left_byte_col.r, left_byte_col.g, left_byte_col.b));
			pixels_data.emplace_back(SDL_MapRGB(pixel_format, nullptr, right_byte_col.r, right_byte_col.g, right_byte_col.b));

			++current_byte;
		}

		BlitRawPixelDataToSurface(surface, bounds, pixels_data);

		SDL_UnlockSurface(surface);
		Renderer::s_sdl_update_mutex.unlock();

		
	}

	void rom::SpinballROM::RenderToSurface(SDL_Surface* surface, size_t offset, Point dimensions) const
	{
		Renderer::s_sdl_update_mutex.lock();
		SDL_LockSurface(surface);

		SDL_ClearSurface(surface, 0, 0, 0, 255);

		const BoundingBox bounds{ 0,0, dimensions.x, dimensions.y };
		const SDL_PixelFormatDetails* pixel_format = SDL_GetPixelFormatDetails(surface->format);

		const Uint8* current_byte = &m_buffer[offset];
		std::vector<Uint32> pixels_data;
		for (int i = 0; i < dimensions.x * dimensions.y; ++i)
		{
			Uint16 colour_data = (static_cast<Uint16>(*(current_byte)) << 8) | static_cast<Uint16>(*(current_byte + 1));
			rom::Colour found_colour{ 0, Colour::levels_lookup[(0x0F00 & colour_data) >> 8], Colour::levels_lookup[(0x00F0 & colour_data) >> 4], Colour::levels_lookup[(0x000F & colour_data)] };
			pixels_data.emplace_back(SDL_MapRGB(pixel_format, nullptr, found_colour.r, found_colour.g, found_colour.b));
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
		
		const SDL_PixelFormatDetails* format_details = SDL_GetPixelFormatDetails(surface->format);

		const size_t pixel_count_pitch = surface->pitch / format_details->bytes_per_pixel;
		size_t target_pixel_index = (y_off * pixel_count_pitch) + x_off;
		for (size_t i = 0; i < pixels_data.size() && i < pixel_count_pitch * surface->h && i / x_max < y_max; ++i, target_pixel_index += 1)
		{
			if ((i % bounds.Width()) == 0)
			{
				target_pixel_index = (y_off * pixel_count_pitch) + (pixel_count_pitch * (i / bounds.Width())) + x_off;
			}
			rom::Swatch pixel_as_swatch = { static_cast<Uint16>(pixels_data[i]) };
			//((Uint32*)surface->pixels)[target_pixel_index] = SDL_MapRGB(SDL_GetPixelFormatDetails(surface->format), NULL, static_cast<Uint8>(pixel_as_swatch.GetUnpacked().r / 15.0f) * 255, static_cast<Uint8>(pixel_as_swatch.GetUnpacked().g / 15.0f) * 255, static_cast<Uint8>(pixel_as_swatch.GetUnpacked().b / 15.0f) * 255);
			((Uint32*)surface->pixels)[target_pixel_index] = pixels_data[i];
		}
	}
}