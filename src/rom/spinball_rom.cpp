#include "rom/spinball_rom.h"
#include "render.h"

#include "rom/sprite.h"
#include "rom/palette.h"
#include "types/sdl_handle_defs.h"

#include <fstream>
#include "SDL3/SDL_stdinc.h"
#include "SDL3/SDL_surface.h"

namespace
{
    template<typename T>
    Uint32 ReadValue(const std::vector<unsigned char> &buffer, Uint32 offset, T &out_value)
    {
        const auto new_offset = offset + sizeof(T);
        if (new_offset < buffer.size())
        {
            out_value = 0;
            if constexpr(sizeof(T) > 1)
            {
                for (auto i = 1; i < sizeof(T); ++i)
                {
                    out_value += static_cast<T>(buffer[offset + (i-1)] << (8*i));
                }
            }
            out_value += static_cast<T>(buffer[offset]);
        }
        return new_offset;
    }

    template<typename T>
    [[nodiscard]] T ReadValue(Uint32 offset)
    {
        T out_value = 0;
        ReadValue<T>(offset, out_value);
        return out_value;
    }

    template<typename T>
    Uint32 WriteValue(std::vector<unsigned char>& buffer, Uint32 offset, T in_value)
    {
        const auto new_offset = offset + 2;
        if ((new_offset) <= buffer.size())
        {
            if constexpr(sizeof(T) > 1)
            {
                for (auto i = 0; i < sizeof(T); ++i)
                {
                    buffer[offset + i]  = (in_value & (static_cast<T>(0xFF) << (8 * (i - sizeof(T)))));
                }
            }
        }

        return new_offset;
    }
}

namespace spintool
{
    bool rom::SpinballROM::LoadROMFromPath(const std::filesystem::path &path)
    {
        std::ifstream input = std::ifstream{path, std::ios::binary};
        m_filepath = path;
        m_buffer = std::vector<unsigned char>{std::istreambuf_iterator<char>(input), {}};
        m_palettes = LoadPalettes(48);

        return m_buffer.empty() == false;
    }

    void rom::SpinballROM::SaveROM()
    {
        std::ofstream output = std::ofstream{m_filepath, std::ios::binary};
        output.write(reinterpret_cast<const char *>(m_buffer.data()), m_buffer.size());
    }

    Uint32 rom::SpinballROM::GetOffsetForNextSprite(const rom::Sprite &current_sprite) const
    {
        return current_sprite.rom_data.rom_offset + current_sprite.GetSizeOf();
    }

    std::vector<std::shared_ptr<rom::Palette> > rom::SpinballROM::LoadPalettes(Uint32 num_palettes) const
    {
        constexpr Uint32 offset = 0xDFC; // Level Palettes
        //constexpr size_t offset = 0x9BCDA; // Main menu palettes
        //constexpr size_t offset = 0xA1388; // Veg-o-Fortress cutscene palettes
        std::vector<std::shared_ptr<rom::Palette> > results;

        for (Uint32 p = 0; p < num_palettes; ++p)
        {
            results.emplace_back(rom::Palette::LoadFromROM(*this, offset + (Palette::s_palette_size_on_rom * p)));
        }

        for (Uint32 p = 0; p < 2; ++p)
        {
            results.emplace_back(rom::Palette::LoadFromROM(*this, 0x000993FA + (Palette::s_palette_size_on_rom * p)));
        }

        for (Uint32 p = 0; p < 4; ++p)
        {
            results.emplace_back(rom::Palette::LoadFromROM(*this, 0x000A1388 + (Palette::s_palette_size_on_rom * p)));
        }

        // The title screen has its own complete four-line palette set. Keep
        // these lines in the general palette editor as well, so they can be
        // modified with the same colour picker and immediate ROM saving used
        // by the other uncompressed palettes. They are appended to preserve
        // all existing palette indices.
        for (Uint32 p = 0; p < 4; ++p)
        {
            results.emplace_back(rom::Palette::LoadFromROM(
                *this,
                0x0009BD3A + (Palette::s_palette_size_on_rom * p)
            ));
        }
        return results;
    }

    void rom::SpinballROM::RenderToSurface(SDL_Surface *surface, Uint32 offset, Point dimensions, const rom::Palette &palette) const
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

        const BoundingBox bounds{0, 0, dimensions.x, dimensions.y};
        const SDL_PixelFormatDetails *pixel_format = SDL_GetPixelFormatDetails(surface->format);

        const Uint8 *current_byte = &m_buffer[offset];
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

    void rom::SpinballROM::RenderToSurface(SDL_Surface *surface, Uint32 offset, Point dimensions) const
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

        const BoundingBox bounds{0, 0, dimensions.x, dimensions.y};
        const SDL_PixelFormatDetails *pixel_format = SDL_GetPixelFormatDetails(surface->format);

        Uint32 next_offset = offset;
        std::vector<Uint32> pixels_data;
        for (int i = 0; i < dimensions.x * dimensions.y; ++i)
        {
            const Uint16 colour_data = ReadUint16(next_offset);
            const rom::Colour found_colour{0, Colour::levels_lookup[(0x0F00 & colour_data) >> 8], Colour::levels_lookup[(0x00F0 & colour_data) >> 4], Colour::levels_lookup[(0x000F & colour_data)]};
            pixels_data.emplace_back(SDL_MapRGB(pixel_format, nullptr, found_colour.r, found_colour.g, found_colour.b));
            next_offset += 2;
        }

        BlitRawPixelDataToSurface(surface, bounds, pixels_data);

        SDL_UnlockSurface(surface);
        Renderer::s_sdl_update_mutex.unlock();
    }

    void rom::SpinballROM::BlitRawPixelDataToSurface(SDL_Surface *surface, const BoundingBox &bounds, const std::vector<Uint32> &pixels_data) const
    {
        int x_off = 0;
        int y_off = 0;
        int x_max = bounds.Width();
        int y_max = bounds.Height();

        const SDL_PixelFormatDetails *format_details = SDL_GetPixelFormatDetails(surface->format);

        const size_t pixel_count_pitch = surface->pitch / format_details->bytes_per_pixel;
        size_t target_pixel_index = (y_off * pixel_count_pitch) + x_off;
        for (size_t i = 0; i < pixels_data.size() && i < pixel_count_pitch * surface->h && i / x_max < y_max; ++i, target_pixel_index += 1)
        {
            if ((i % bounds.Width()) == 0)
            {
                target_pixel_index = (y_off * pixel_count_pitch) + (pixel_count_pitch * (i / bounds.Width())) + x_off;
            }
            rom::Swatch pixel_as_swatch = {static_cast<Uint16>(pixels_data[i])};
            //((Uint32*)surface->pixels)[target_pixel_index] = SDL_MapRGB(SDL_GetPixelFormatDetails(surface->format), NULL, static_cast<Uint8>(pixel_as_swatch.GetUnpacked().r / 15.0f) * 255, static_cast<Uint8>(pixel_as_swatch.GetUnpacked().g / 15.0f) * 255, static_cast<Uint8>(pixel_as_swatch.GetUnpacked().b / 15.0f) * 255);
            ((Uint32 *) surface->pixels)[target_pixel_index] = pixels_data[i];
        }
    }

    //000bfc50 - Rom palette sets array

    std::shared_ptr<rom::PaletteSet> rom::SpinballROM::GetOptionsScreenPaletteSet() const
    {
        auto palette_set = std::make_shared<rom::PaletteSet>();
        palette_set->palette_lines =
        {
            rom::Palette::LoadFromROM(*this, 0x115C),
            rom::Palette::LoadFromROM(*this, 0x117C),
            rom::Palette::LoadFromROM(*this, 0x119C),
            rom::Palette::LoadFromROM(*this, 0x11BC)
        };
        return palette_set;
    }

    std::shared_ptr<rom::PaletteSet> rom::SpinballROM::GetIntroCutscenePaletteSet() const
    {
        auto palette_set = std::make_shared<rom::PaletteSet>();
        palette_set->palette_lines =
        {
            rom::Palette::LoadFromROM(*this, 0x000A1388),
            rom::Palette::LoadFromROM(*this, 0x000A13A8),
            rom::Palette::LoadFromROM(*this, 0x000A13C8),
            rom::Palette::LoadFromROM(*this, 0x000A13E8)
        };
        return palette_set;
    }

    std::shared_ptr<rom::PaletteSet> rom::SpinballROM::GetMainMenuPaletteSet() const
    {
        auto palette_set = std::make_shared<rom::PaletteSet>();
        palette_set->palette_lines =
        {
            rom::Palette::LoadFromROM(*this, 0x0009BD3A),
            rom::Palette::LoadFromROM(*this, 0x0009BD5A),
            rom::Palette::LoadFromROM(*this, 0x0009BD7A),
            rom::Palette::LoadFromROM(*this, 0x0009BD9A)
        };
        return palette_set;
    }

    std::shared_ptr<rom::PaletteSet> rom::SpinballROM::GetSegaLogoIntroPaletteSet() const
    {
        auto palette_set = std::make_shared<rom::PaletteSet>();
        palette_set->palette_lines =
        {
            rom::Palette::LoadFromROM(*this, 0x000993FA),
            rom::Palette::LoadFromROM(*this, 0x0009941A),
            rom::Palette::LoadFromROM(*this, 0x0009943A),
            rom::Palette::LoadFromROM(*this, 0x0009945A)
        };
        return palette_set;
    }

    [[nodiscard]] Uint8 rom::SpinballROM::ReadUint8(Uint32 offset) const
    {
        Uint8 out_value;
        ReadValue<Uint8>(m_buffer, offset, out_value);
        return out_value;
    }

    [[nodiscard]] Uint16 rom::SpinballROM::ReadUint16(Uint32 offset) const
    {
        Uint16 out_value;
        ReadValue<Uint16>(m_buffer, offset, out_value);
        return out_value;
    }

    [[nodiscard]] Uint32 rom::SpinballROM::ReadUint32(Uint32 offset) const
    {
        Uint32 out_value;
        ReadValue<Uint32>(m_buffer, offset, out_value);
        return out_value;
    }

    [[nodiscard]] Sint8 rom::SpinballROM::ReadSint8(Uint32 offset) const
    {
        Sint8 out_value;
        ReadValue<Sint8>(m_buffer, offset, out_value);
        return out_value;
    }

    [[nodiscard]] Sint16 rom::SpinballROM::ReadSint16(Uint32 offset) const
    {
        Sint16 out_value;
        ReadValue<Sint16>(m_buffer, offset, out_value);
        return out_value;
    }

    [[nodiscard]] Sint32 rom::SpinballROM::ReadSint32(Uint32 offset) const
    {
        Sint32 out_value;
        ReadValue<Sint32>(m_buffer, offset, out_value);
        return out_value;
    }

    Uint32 rom::SpinballROM::ReadUint8(Uint32 offset, Uint8& out_value) const
    {
        return ReadValue<Uint8>(m_buffer, offset, out_value);
    }
    Uint32 rom::SpinballROM::ReadUint16(Uint32 offset, Uint16& out_value) const
    {
        return ReadValue<Uint16>(m_buffer, offset, out_value);
    }
    Uint32 rom::SpinballROM::ReadUint32(Uint32 offset, Uint32& out_value) const
    {
        return ReadValue<Uint32>(m_buffer, offset, out_value);
    }

    Uint32 rom::SpinballROM::ReadSint8(Uint32 offset, Sint8& out_value) const
    {
        return ReadValue<Sint8>(m_buffer, offset, out_value);
    }
    Uint32 rom::SpinballROM::ReadSint16(Uint32 offset, Sint16& out_value) const
    {
        return ReadValue<Sint16>(m_buffer, offset, out_value);
    }

    Uint32 rom::SpinballROM::ReadSint32(Uint32 offset, Sint32& out_value) const
    {
        return ReadValue<Sint32>(m_buffer, offset, out_value);
    }

    const std::vector<std::shared_ptr<spintool::rom::Palette> > &rom::SpinballROM::GetGlobalPalettes() const
    {
        return m_palettes;
    }

    Uint32 rom::SpinballROM::WriteUint8(Uint32 offset, Uint8 value)
    {
        return WriteValue<Uint8>(m_buffer, offset, value);
    }

    Uint32 rom::SpinballROM::WriteUint16(Uint32 offset, Uint16 value)
    {
       return WriteValue<Uint16>(m_buffer, offset, value);
    }

    Uint32 rom::SpinballROM::WriteUint32(Uint32 offset, Uint32 value)
    {
        return WriteValue<Uint32>(m_buffer, offset, value);
    }

    Uint32 rom::SpinballROM::WriteSint8(Uint32 offset, Sint8 value)
    {
        return WriteValue<Sint8>(m_buffer, offset, value);
    }

    Uint32 rom::SpinballROM::WriteSint16(Uint32 offset, Sint16 value)
    {
        return WriteValue<Sint16>(m_buffer, offset, value);
    }

    Uint32 rom::SpinballROM::WriteSint32(Uint32 offset, Sint32 value)
    {
        return WriteValue<Sint32>(m_buffer, offset, value);
    }
}
}
