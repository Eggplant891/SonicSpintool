#include "spinball_rom.h"

#include "render.h"
#include "sdl_handle_defs.h"

#include <fstream>

namespace spintool
{

	bool SpinballROM::LoadROM()
	{
		std::ifstream input = std::ifstream{ "E:/Development/_RETRODEV/MegaDrive/Spinball/SpinballDisassembly/bin/original/Sonic The Hedgehog Spinball (USA).md", std::ios::binary };
		//std::ifstream input = std::ifstream{ "E:/Development/_RETRODEV/MegaDrive/Spinball/SpinballDisassembly/bin/prototype/Sonic the Hedgehog Spinball (Aug 1993 prototype).md", std::ios::binary };
		//std::ifstream input = std::ifstream{ "E:/Development/_RETRODEV/MegaDrive/Spinball/SpinballDisassembly/bin/prototype/Spinball _ 19-10.bin", std::ios::binary };
		m_buffer = std::vector<unsigned char>{ std::istreambuf_iterator<char>(input), {} };

		return m_buffer.empty() == false;
	}

	bool SpinballROM::LoadROMFromPath(const char* path)
	{
		//std::ifstream input = std::ifstream{ "E:/Development/_RETRODEV/MegaDrive/Spinball/SpinballDisassembly/bin/sbbuilt.bin", std::ios::binary };
		//std::ifstream input = std::ifstream{ "E:/Development/_RETRODEV/MegaDrive/Spinball/SpinballDisassembly/bin/prototype/Sonic the Hedgehog Spinball (Aug 1993 prototype).md", std::ios::binary };
		std::ifstream input = std::ifstream{ path, std::ios::binary };
		m_buffer = std::vector<unsigned char>{ std::istreambuf_iterator<char>(input), {} };

		return m_buffer.empty() == false;
	}

	std::optional<size_t> SpinballROM::FindOffsetForNextSprite(size_t offset)
	{
		for (size_t i = offset; i < m_buffer.size() - 5; ++i)
		{
			if ((m_buffer[i] == 0xFF || m_buffer[i] == 0x00) && (m_buffer[i+1] == 0xFF || m_buffer[i+2] == 0x00))
			{
				if (m_buffer[i + 4] > 0 && m_buffer[i + 5] > 0 && (m_buffer[i + 4] <= 128 || m_buffer[i + 5] <= 128))
				{
					const float x_size = static_cast<float>(m_buffer[i + 5] * 2);
					const float y_size = static_cast<float>(m_buffer[i + 4]);

					constexpr float x_aspect_threshold = 64.0f / 16.0f;
					constexpr float y_aspect_threshold = 16.0f / 64.0f;

					float x_aspect = m_buffer[i + 4] / static_cast<float>(m_buffer[i + 5] * 2);


					if (x_aspect <= x_aspect_threshold && x_aspect >= y_aspect_threshold)
					{
						return i;
					}
				}
			}
		}
		return std::nullopt;
	}

	std::optional<SpinballSprite> SpinballROM::LoadSprite(size_t offset, bool packed_data_mode, bool try_to_read_missed_data)
	{
		if (offset + 6 >= m_buffer.size())
		{
			return std::nullopt;
		}

		unsigned char* current_byte = &m_buffer[offset];
		//unsigned char* current_byte = &buffer[0x1CE0];

		SpinballSprite new_sprite;

		new_sprite.rom_offset = current_byte - m_buffer.data();

		new_sprite.x_offset = (static_cast<Sint16>(*(current_byte )) << 8) | static_cast<Sint16>(*(current_byte + 1));
		current_byte += 2;

		new_sprite.y_offset = (static_cast<Sint16>(*(current_byte )) << 8) | static_cast<Sint16>(*(current_byte + 1));
		current_byte += 2;

		new_sprite.y_size = *current_byte;
		++current_byte;

		new_sprite.x_size = *current_byte * 2;
		++current_byte;

		const size_t total_pixels = new_sprite.x_size * new_sprite.y_size;
		if (total_pixels != 0)
		{
			new_sprite.pixel_data.reserve(total_pixels);

			for (size_t i = 0; i < total_pixels && offset + i < m_buffer.size(); ++i)
			{
				if (packed_data_mode)
				{
					const Uint32 left_byte = (0xF0 & *current_byte) >> 4;
					const Uint32 right_byte = 0x0F & *current_byte;

					new_sprite.pixel_data.emplace_back(left_byte);
					new_sprite.pixel_data.emplace_back(right_byte);

					++current_byte;
				}
				else
				{
					new_sprite.pixel_data.emplace_back(*current_byte);
					++current_byte;
				}
			}
		}
		new_sprite.real_size = current_byte - &m_buffer[offset];

		if (try_to_read_missed_data)
		{
			const size_t current_offset = current_byte - m_buffer.data();
			for (size_t i = current_offset; i < FindOffsetForNextSprite(current_offset); ++i)
			{
				if (packed_data_mode)
				{
					const Uint32 left_byte = (0xF0 & *current_byte) >> 4;
					const Uint32 right_byte = 0x0F & *current_byte;

					new_sprite.pixel_data.emplace_back(left_byte);
					new_sprite.pixel_data.emplace_back(right_byte);

					++current_byte;
				}
				else
				{
					new_sprite.pixel_data.emplace_back(*current_byte);
					++current_byte;
				}
			}
			new_sprite.unrealised_size = (current_byte - &m_buffer[offset]) - new_sprite.real_size;
		}

		if (new_sprite.x_size == 0 && new_sprite.y_size == 0)
		{
			return std::nullopt;
		}

		if ((new_sprite.x_offset >= -32 || new_sprite.x_offset <= 32) && (new_sprite.y_offset >= -32 || new_sprite.y_offset <= 32))
		{
			if (new_sprite.x_size > 0 && new_sprite.y_size > 0 && (new_sprite.x_size <= 256 || new_sprite.y_size <= 256))
			{
				constexpr float x_aspect_threshold = 64.0f / 16.0f;
				constexpr float y_aspect_threshold = 16.0f / 64.0f;

				float x_aspect = new_sprite.y_size / static_cast<float>(new_sprite.x_size);

				if (x_aspect <= x_aspect_threshold && x_aspect >= y_aspect_threshold)
				{
					return new_sprite;
				}
			}
		}

		return std::nullopt;
	}

	std::vector<spintool::VDPPalette> SpinballROM::LoadPalettes(size_t num_palettes)
	{
		constexpr size_t offset = 0xDFC;
		unsigned char* current_byte = &m_buffer[offset];
		std::vector<spintool::VDPPalette> results;

		for (size_t p = 0; p < num_palettes; ++p)
		{
			VDPPalette palette;
			palette.offset = current_byte - m_buffer.data();

			for (size_t i = 0; i < 16; ++i)
			{
				const Uint8 first_byte = *current_byte;
				++current_byte;
				const Uint8 second_byte = *current_byte;
				++current_byte;
				VDPSwatch swatch;
				swatch.packed_value = static_cast<Uint16>((static_cast<Uint16>(first_byte) << 8) | second_byte);

				palette.palette_swatches[i] = swatch;
			}
			results.emplace_back(palette);
		}

		return std::move(results);
	}

	void SpinballSprite::RenderToSurface(SDL_Surface* surface) const
	{
		Renderer::s_sdl_update_mutex.lock();
		SDL_LockSurface(surface);

		size_t target_pixel_index = 0;
		SDL_ClearSurface(surface, 0, 0, 0, 255);

		if (x_size > 0 && y_size > 0)
		{
			for (size_t i = 0; i < pixel_data.size() && i < surface->pitch * surface->h && i / x_size < y_size; ++i, target_pixel_index += 1)
			{
				if ((i % x_size) == 0) target_pixel_index = (surface->pitch * (i / x_size));

				//*(Uint32*)(&((Uint8*)surface->pixels)[target_pixel_index]) = SDL_MapRGB(Renderer::s_pixel_format_details, NULL, static_cast<Uint8>((pixel_data[i] / 16.0f) * 255), 0, 0);
				((Uint8*)surface->pixels)[target_pixel_index] = pixel_data[i];
			}
		}

		SDL_UnlockSurface(surface);
		Renderer::s_sdl_update_mutex.unlock();
	}
}