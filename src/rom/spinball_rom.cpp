#include "rom/spinball_rom.h"
#include "render.h"

#include "rom/sprite.h"
#include "rom/palette.h"
#include "types/sdl_handle_defs.h"

#include <fstream>
#include "SDL3/SDL_stdinc.h"
#include "SDL3/SDL_surface.h"


namespace spintool
{
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
		return current_sprite.rom_data.rom_offset + current_sprite.GetSizeOf();
	}

	std::vector<std::shared_ptr<rom::Palette>> rom::SpinballROM::LoadPalettes(size_t num_palettes)
	{
		constexpr size_t offset = 0xDFC; // Level Palettes
		//constexpr size_t offset = 0x9BCDA; // Main menu palettes
		//constexpr size_t offset = 0xA1388; // Veg-o-Fortress cutscene palettes
		std::vector<std::shared_ptr<rom::Palette>> results;

		for (size_t p = 0; p < num_palettes; ++p)
		{
			results.emplace_back(rom::Palette::LoadFromROM(*this, offset + (Palette::s_palette_size_on_rom * p)));
		}

		for (size_t p = 0; p < 2; ++p)
		{
			results.emplace_back(rom::Palette::LoadFromROM(*this, 0x000993FA + (Palette::s_palette_size_on_rom * p)));
		}

		for (size_t p = 0; p < 4; ++p)
		{
			results.emplace_back(rom::Palette::LoadFromROM(*this, 0x000A1388 + (Palette::s_palette_size_on_rom * p)));
		}
		return results;
	}

	void rom::SpinballROM::RenderToSurface(SDL_Surface* surface, size_t offset, Point dimensions, const rom::Palette& palette) const
	{
		if (dimensions.x <= 0 || dimensions.y <= 0)
		{
			return;
		}

		if (offset >= m_buffer.size())
		{
			return;
		}

		Renderer::s_sdl_update_mutex.lock();
		SDL_LockSurface(surface);

		SDL_ClearSurface(surface, 0, 0, 0, 255);

		const BoundingBox bounds{ 0,0, dimensions.x, dimensions.y };
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
		if (dimensions.x <= 0 || dimensions.y <= 0)
		{
			return;
		}

		if (offset >= m_buffer.size())
		{
			return;
		}

		Renderer::s_sdl_update_mutex.lock();
		SDL_LockSurface(surface);

		SDL_ClearSurface(surface, 0, 0, 0, 255);

		const BoundingBox bounds{ 0,0, dimensions.x, dimensions.y };
		const SDL_PixelFormatDetails* pixel_format = SDL_GetPixelFormatDetails(surface->format);

		size_t next_offset = offset;
		std::vector<Uint32> pixels_data;
		for (int i = 0; i < dimensions.x * dimensions.y; ++i)
		{
			const Uint16 colour_data = ReadUint16(next_offset);
			const rom::Colour found_colour{ 0, Colour::levels_lookup[(0x0F00 & colour_data) >> 8], Colour::levels_lookup[(0x00F0 & colour_data) >> 4], Colour::levels_lookup[(0x000F & colour_data)] };
			pixels_data.emplace_back(SDL_MapRGB(pixel_format, nullptr, found_colour.r, found_colour.g, found_colour.b));
			next_offset += 2;
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

	//000bfc50 - Rom palette sets array

	std::shared_ptr<rom::PaletteSet> rom::SpinballROM::GetOptionsScreenPaletteSet() const
	{
		static std::shared_ptr<rom::PaletteSet> palette_set = [this]()
			{
				auto out_set = std::make_shared<rom::PaletteSet>();
				out_set->palette_lines =
				{
					rom::Palette::LoadFromROM(*this, 0x115C),
					rom::Palette::LoadFromROM(*this, 0x117C),
					rom::Palette::LoadFromROM(*this, 0x119C),
					rom::Palette::LoadFromROM(*this, 0x11BC)
				};
				return std::move(out_set);
			}();
		

		return palette_set;
	}

	std::shared_ptr<rom::PaletteSet> rom::SpinballROM::GetIntroCutscenePaletteSet() const
	{
		static std::shared_ptr<rom::PaletteSet> palette_set = [this]()
			{
				auto out_set = std::make_shared<rom::PaletteSet>();
				out_set->palette_lines =
				{
					rom::Palette::LoadFromROM(*this,0x000A1388),
					rom::Palette::LoadFromROM(*this,0x000A13A8),
					rom::Palette::LoadFromROM(*this,0x000A13C8),
					rom::Palette::LoadFromROM(*this,0x000A13E8)
				};
				return std::move(out_set);
			}();


		return palette_set;
	}

	std::shared_ptr<rom::PaletteSet> rom::SpinballROM::GetMainMenuPaletteSet() const
	{
		static std::shared_ptr<rom::PaletteSet> palette_set = [this]()
			{
				auto out_set = std::make_shared<rom::PaletteSet>();
				out_set->palette_lines =
				{
					rom::Palette::LoadFromROM(*this,0x0009bd3a),
					rom::Palette::LoadFromROM(*this,0x0009bd5a),
					rom::Palette::LoadFromROM(*this,0x0009bd7a),
					rom::Palette::LoadFromROM(*this,0x0009bd9a)
				};
				return std::move(out_set);
			}();


		return palette_set;
	}

	std::shared_ptr<rom::PaletteSet> rom::SpinballROM::GetSegaLogoIntroPaletteSet() const
	{
		static std::shared_ptr<rom::PaletteSet> palette_set = [this]()
			{
				auto out_set = std::make_shared<rom::PaletteSet>();
				out_set->palette_lines =
				{
					rom::Palette::LoadFromROM(*this,0x000993FA),
					rom::Palette::LoadFromROM(*this,0x0009941A),
					rom::Palette::LoadFromROM(*this,0x0009943A),
					rom::Palette::LoadFromROM(*this,0x0009945A)
				};
				return std::move(out_set);
			}();


		return palette_set;
	}

	Uint32 rom::SpinballROM::ReadUint32(size_t offset) const
	{
		if (offset + 4 <= m_buffer.size())
		{
			Uint32 out_offset = (static_cast<Uint32>(m_buffer[offset]) << 24) | (static_cast<Uint32>(m_buffer[offset+1]) << 16) | (static_cast<Uint32>(m_buffer[offset+2]) << 8) | (static_cast<Uint32>(m_buffer[offset+3]));
			return out_offset;
		}
		return 0;
	}

	Uint16 rom::SpinballROM::ReadUint16(size_t offset) const
	{
		if (offset + 2 <= m_buffer.size())
		{
			Uint16 out_offset = (static_cast<Uint16>(m_buffer[offset]) << 8) | (static_cast<Uint16>(m_buffer[offset + 1]));
			return out_offset;
		}
		return 0;
	}

	Uint8 rom::SpinballROM::ReadUint8(size_t offset) const
	{
		if (offset < m_buffer.size())
		{
			Uint8 out_offset = static_cast<Uint16>(m_buffer[offset]);
			return out_offset;
		}
		return 0;
	}
}