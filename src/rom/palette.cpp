#include "rom/palette.h"

#include "rom/spinball_rom.h"

#include "imgui.h"

#include <algorithm>
#include <cstdlib>
#include <limits>

namespace spintool::rom
{
	std::shared_ptr<spintool::rom::Palette> Palette::LoadFromROM(const SpinballROM& src_rom, Uint32 offset)
	{
		if (offset > src_rom.m_buffer.size() ||
			s_palette_size_on_rom > src_rom.m_buffer.size() - static_cast<size_t>(offset))
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
		// ImGui's colour picker returns arbitrary RGB values, while the ROM can
		// only store the discrete colour levels supported by the Mega Drive.
		// The former implementation required an exact match with one of those
		// levels, so almost every picked value was converted to zero. Quantise
		// each component to the nearest representable level instead.
		auto nearest_level = [](float component) -> Uint8
		{
			const int target = static_cast<int>(
				std::clamp(component, 0.0f, 1.0f) * 255.0f + 0.5f
			);

			Uint8 best_index = 0;
			int best_distance = std::numeric_limits<int>::max();
			for (std::size_t index = 0; index < Colour::levels_lookup.size(); ++index)
			{
				const int distance = std::abs(
					target - static_cast<int>(Colour::levels_lookup[index])
				);
				if (distance < best_distance)
				{
					best_distance = distance;
					best_index = static_cast<Uint8>(index);
				}
			}
			return best_index;
		};

		const Uint8 red = nearest_level(r);
		const Uint8 green = nearest_level(g);
		const Uint8 blue = nearest_level(b);

		return static_cast<Uint16>(
			(static_cast<Uint16>(blue) << 8U) |
			(static_cast<Uint16>(green) << 4U) |
			static_cast<Uint16>(red)
		);
	}

	ImColor rom::Swatch::AsImColor() const
	{
		float col[3] = { static_cast<float>(GetUnpacked().r / 255.0f), static_cast<float>(GetUnpacked().g / 255.0f), static_cast<float>(GetUnpacked().b / 255.0f) };
		return ImColor{ col[0], col[1], col[2], 1.0f };
	}

	std::shared_ptr<spintool::rom::PaletteSet> PaletteSet::LoadFromROM(const SpinballROM& src_rom, Uint32 offset)
	{
		constexpr size_t root_palette = 0xDFC;

		if (offset > src_rom.m_buffer.size() ||
			8 > src_rom.m_buffer.size() - static_cast<size_t>(offset))
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

		for (const auto& palette : palette_set->palette_lines)
		{
			if (!palette)
			{
				return nullptr;
			}
		}

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
