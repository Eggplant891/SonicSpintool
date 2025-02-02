#pragma once

#include "rom/rom_data.h"
#include "types/decompression_result.h"

#include "SDL3/SDL_stdinc.h"

#include <vector>
#include <memory>

namespace spintool::rom
{
	class SpinballROM;
	struct Sprite;
	struct SpriteTile;
	struct TileSet;
}

namespace spintool
{
	struct TilesetEntry
	{
		std::shared_ptr<const rom::TileSet> tileset;
		DecompressionResult result;
	};
}

namespace spintool::rom
{
	struct TileSet
	{
		ROMData rom_data;
		size_t compressed_size = 0;
		size_t uncompressed_size = 0;

		Uint16 num_tiles = 0;
		std::vector<Uint8> uncompressed_data;

		static TilesetEntry LoadFromROM(const SpinballROM& src_rom, size_t rom_offset);
		static TilesetEntry LoadFromROMSecondCompression(const SpinballROM& src_rom, size_t rom_offset);
		std::shared_ptr<const Sprite> CreateSpriteFromTile(const size_t offset) const;
		std::shared_ptr<SpriteTile> CreateSpriteTileFromTile(const size_t tile_index) const;
		constexpr const static Uint16 s_tile_width = 0x08;
		constexpr const static Uint16 s_tile_height = 0x08;
		constexpr const static Uint16 s_tile_total_pixels = s_tile_width * s_tile_height;
		constexpr const static Uint16 s_tile_total_bytes = (s_tile_width / 2) * s_tile_height;
	};
}