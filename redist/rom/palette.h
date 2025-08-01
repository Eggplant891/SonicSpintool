#pragma once

#include "rom/colour.h"

#include "SDL3/SDL_stdinc.h"
#include <array>
#include <memory>
#include "types/rom_ptr.h"

struct ImColor;

namespace spintool::rom
{
	class SpinballROM;
}

namespace spintool::rom
{
	struct Swatch
	{
		Uint16 packed_value;

		Colour GetUnpacked() const;
		static Uint16 Pack(float r, float g, float b);
		ImColor AsImColor() const;
	};

	struct Palette
	{
		constexpr static const Uint32 s_swatches_per_palette = 16;
		constexpr static const Uint32 s_size_per_swatch = 2;
		constexpr static const Uint32 s_palette_size_on_rom = s_swatches_per_palette * s_size_per_swatch;
		
		std::array<Swatch, s_swatches_per_palette> palette_swatches;
		Uint32 offset;

		static std::shared_ptr<Palette> LoadFromROM(const SpinballROM& src_rom, Uint32 offset);

	};

	struct PaletteSet
	{
		PaletteSetArray palette_lines;

		static std::shared_ptr<PaletteSet> LoadFromROM(const SpinballROM& src_rom, Uint32 offset);

		bool operator==(const PaletteSet& rhs);
	};
}