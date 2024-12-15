#pragma once

#include "SDL3/SDL.h"
#include "sdl_handle_defs.h"
#include "render.h"

#include <vector>
#include <memory>
#include <optional>
#include "imgui.h"

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
		static Uint16 Pack(float r, float g, float b);
		ImColor AsImColor() const;
	};

	struct VDPPalette
	{
		VDPSwatch palette_swatches[16];
		size_t offset;
	};

	struct BoundingBox
	{
		Point min{0,0};
		Point max{0,0};

		int Width() const;
		int Height() const;
	};

	struct SpriteTile
	{
		Sint16 x_offset = 0;
		Sint16 y_offset = 0;

		Uint8 x_size = 0;
		Uint8 y_size = 0;
		Uint8 padding_0 = 0; // For padding, not real data
		Uint8 padding_1 = 0; // For padding, not real data

		size_t rom_offset = 0;
		size_t rom_offset_end = 0;
		std::vector<Uint32> pixel_data;

		void RenderToSurface(SDL_Surface* surface) const;
		void BlitPixelDataToSurface(SDL_Surface* surface, const BoundingBox& bounds, const std::vector<Uint32>& pixels_data) const;
	};

	struct SpinballSprite
	{
		size_t rom_offset = 0;
		size_t rom_offset_end = 0;
		size_t real_size = 0;
		size_t unrealised_size = 0;

		Uint16 num_tiles = 0;
		Uint16 num_vdp_tiles = 0;
		
		std::vector<std::shared_ptr<SpriteTile>> sprite_tiles;

		void RenderToSurface(SDL_Surface* surface) const;

		BoundingBox GetBoundingBox() const;
		size_t GetSizeOf() const; // Size in bytes on ROM
	};

	struct IntroGraphic
	{
		size_t rom_offset;
		size_t real_size;

	};

	struct SpinballLevelTileData
	{
		size_t rom_offset = 0;
		size_t rom_offset_end = 0;

		size_t compressed_size = 0;
		size_t uncompressed_size = 0;

		Uint16 num_tiles;
		std::vector<unsigned char> data;
	};

	class SpinballROM
	{
	public:
		void LoadTileData(size_t rom_offset);
		bool LoadROM();
		void SaveROM();
		bool LoadROMFromPath(const char* path);
		size_t GetOffsetForNextSprite(const SpinballSprite& current_sprite) const;
		std::shared_ptr<const SpinballSprite> LoadSprite(const size_t offset, bool packed_data_mode, bool try_to_read_missed_data);
		std::shared_ptr<const SpinballSprite> LoadLevelTile(const std::vector<unsigned char>& data_source, const size_t offset);
		std::vector<VDPPalette> LoadPalettes(size_t num_palettes);

		void RenderToSurface(SDL_Surface* surface, size_t offset, Point dimensions) const;
		void BlitRawPixelDataToSurface(SDL_Surface* surface, const BoundingBox& bounds, const std::vector<Uint32>& pixels_data) const;

		std::vector<unsigned char> m_buffer;
		SpinballLevelTileData m_toxic_caves_bg_data;
	};
}