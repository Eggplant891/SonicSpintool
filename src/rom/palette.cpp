#include "rom/palette.h"

#include "rom/spinball_rom.h"

#include "imgui.h"

namespace spintool::rom
{
	std::shared_ptr<spintool::rom::Palette> Palette::LoadFromROM(const SpinballROM& src_rom, Uint32 offset)
	{
		if (offset >= src_rom.m_buffer.size() || offset + s_palette_size_on_rom >= src_rom.m_buffer.size())
		{
			return nullptr;
		}

		std::shared_ptr<Palette> new_palette = std::make_unique<Palette>();

		new_palette->offset = offset;

		for (Swatch& palette_swatch : new_palette->palette_swatches)
		{
			palette_swatch.packed_value = src_rom.ReadUint16(offset);
			offset += 2;
		}

		return new_palette;
	}

	rom::Colour Swatch::GetUnpacked() const
	{
		return rom::Colour{ 0xFF, Colour::levels_lookup[(0x0F00 & packed_value) >> 8], Colour::levels_lookup[(0x00F0 & packed_value) >> 4], Colour::levels_lookup[(0x000F & packed_value)] };
	}

	Uint16 Swatch::Pack(float r, float g, float b)
	{
		const auto red_it = std::find(std::begin(Colour::levels_lookup), std::end(Colour::levels_lookup), static_cast<Uint8>(std::clamp(r * 255.0f, 0.0f, 255.0f)));
		const auto green_it = std::find(std::begin(Colour::levels_lookup), std::end(Colour::levels_lookup), static_cast<Uint8>(std::clamp(g * 255.0f, 0.0f, 255.0f)));
		const auto blue_it = std::find(std::begin(Colour::levels_lookup), std::end(Colour::levels_lookup), static_cast<Uint8>(std::clamp(b * 255.0f, 0.0f, 255.0f)));

		const Uint8 red = red_it != std::end(Colour::levels_lookup) ? static_cast<Uint8>(std::distance(std::begin(Colour::levels_lookup), red_it)) : 0;
		const Uint8 green = green_it != std::end(Colour::levels_lookup) ? static_cast<Uint8>(std::distance(std::begin(Colour::levels_lookup), green_it)) : 0;
		const Uint8 blue = blue_it != std::end(Colour::levels_lookup) ? static_cast<Uint8>(std::distance(std::begin(Colour::levels_lookup), blue_it)) : 0;

		return static_cast<Uint16>(0x0F00 & (blue << 8)) | static_cast<Uint16>(0x00F0 & (green << 4)) | static_cast<Uint16>(0x000F & red);
	}

	ImColor rom::Swatch::AsImColor() const
	{
		float col[3] = { static_cast<float>(GetUnpacked().r / 255.0f), static_cast<float>(GetUnpacked().g / 255.0f), static_cast<float>(GetUnpacked().b / 255.0f) };
		return ImColor{ col[0], col[1], col[2], 1.0f };
	}

	std::shared_ptr<spintool::rom::PaletteSet> PaletteSet::LoadFromROM(const SpinballROM& src_rom, Uint32 offset)
	{
		constexpr size_t root_palette = 0xDFC;

		if (offset + 2 * 4 >= src_rom.m_buffer.size())
		{
			return nullptr;
		}

		std::shared_ptr<rom::PaletteSet> palette_set = std::make_shared<rom::PaletteSet>();

		palette_set->palette_lines =
		{
			rom::Palette::LoadFromROM(src_rom, root_palette + (src_rom.ReadUint16(offset) * 0x20)),
			rom::Palette::LoadFromROM(src_rom, root_palette + (src_rom.ReadUint16(offset + 2) * 0x20)),
			rom::Palette::LoadFromROM(src_rom, root_palette + (src_rom.ReadUint16(offset + 4) * 0x20)),
			rom::Palette::LoadFromROM(src_rom, root_palette + (src_rom.ReadUint16(offset + 6) * 0x20))
		};

		return palette_set;
	}

	bool PaletteSet::operator==(const PaletteSet& rhs)
	{
		for (Uint32 i = 0; i < palette_lines.size(); ++i)
		{
			if (palette_lines[i] != nullptr && palette_lines[i].get() != rhs.palette_lines[i].get())
			{
				return false;
			}
		}
		return true;
	}

}