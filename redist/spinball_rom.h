#pragma once

#include "SDL3/SDL.h"
#include "sdl_handle_defs.h"
#include "render.h"

#include <vector>
#include <memory>
#include <optional>

namespace spintool
{
	struct VDPColour
	{
		Uint8 x;
		Uint8 b;
		Uint8 g;
		Uint8 r;
	};

	struct VDPSwatch
	{
		Uint16 packed_value;

		VDPColour GetUnpacked() const;
	};

	struct VDPPalette
	{
		VDPSwatch palette_swatches[16];
		size_t offset;
	};

	struct SpinballSprite
	{
		size_t rom_offset = 0;
		size_t real_size = 0;
		size_t unrealised_size = 0;

		Sint16 x_offset = 0;
		Sint16 y_offset = 0;

		Uint8 x_size = 0;
		Uint8 y_size = 0;
		Uint8 z_size = 0;
		Uint8 w_size = 0;

		std::vector<Uint32> pixel_data;
		void RenderToSurface(SDL_Surface* surface) const;
	};

	class SpinballROM
	{
	public:
		bool LoadROM();
		bool LoadROMFromPath(const char* path);
		std::optional<size_t> FindOffsetForNextSprite(size_t offset);
		std::optional<SpinballSprite> LoadSprite(const size_t offset, bool packed_data_mode, bool try_to_read_missed_data);
		std::vector<VDPPalette> LoadPalettes(size_t num_palettes);

		std::vector<unsigned char> m_buffer;
	};
}