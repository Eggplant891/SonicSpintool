#include "rom/palette.h"

#include "rom/spinball_rom.h"

#include "imgui.h"

namespace spintool::rom
{
	std::shared_ptr<spintool::rom::Palette> Palette::LoadFromROM(const SpinballROM& src_rom, size_t offset)
	{
		if (offset >= src_rom.m_buffer.size() || offset + s_palette_size_on_rom >= src_rom.m_buffer.size())
		{
			return nullptr;
		}

		std::shared_ptr<Palette> new_palette = std::make_unique<Palette>();

		new_palette->offset = offset;

		const Uint8* current_byte = &src_rom.m_buffer[offset];

		for (Swatch& palette_swatch : new_palette->palette_swatches)
		{
			const Uint8 first_byte = *current_byte;
			++current_byte;
			const Uint8 second_byte = *current_byte;
			++current_byte;

			palette_swatch.packed_value = static_cast<Uint16>((static_cast<Uint16>(first_byte) << 8) | second_byte);
		}

		return new_palette;
	}

	rom::Colour Swatch::GetUnpacked() const
	{
		return rom::Colour{ 0xFF, Colour::levels_lookup[(0x0F00 & packed_value) >> 8], Colour::levels_lookup[(0x00F0 & packed_value) >> 4], Colour::levels_lookup[(0x000F & packed_value)] };
	}

	Uint16 Swatch::Pack(float r, float g, float b)
	{
		Uint8 red = static_cast<Uint8>(r * 15);
		Uint8 green = static_cast<Uint8>(g * 15);
		Uint8 blue = static_cast<Uint8>(b * 15);

		return static_cast<Uint16>((0x0F00 & Colour::levels_lookup[blue]) << 8) | static_cast<Uint16>((0x00F0 & Colour::levels_lookup[green]) << 4) | static_cast<Uint16>(0x000F & Colour::levels_lookup[red]);
	}

	ImColor rom::Swatch::AsImColor() const
	{
		float col[3] = { static_cast<float>(GetUnpacked().r / 255.0f), static_cast<float>(GetUnpacked().g / 255.0f), static_cast<float>(GetUnpacked().b / 255.0f) };
		return ImColor{ col[0], col[1], col[2], 1.0f };
	}
}