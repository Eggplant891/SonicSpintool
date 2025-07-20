#pragma once

#include "rom/rom_data.h"
#include "types/decompression_result.h"

#include "SDL3/SDL_stdinc.h"
#include "types/sdl_handle_defs.h"

#include <vector>
#include <memory>

namespace spintool::rom
{
	class SpinballROM;
	struct Sprite;
	struct SpriteTile;
	struct Tile;
	struct TileSet;
}

namespace spintool
{
	struct TilesetEntry
	{
		std::shared_ptr<const rom::TileSet> tileset;
		DecompressionResult result;
	};

	struct TileBrushPreview
	{
		SDLSurfaceHandle surface;
		SDLTextureHandle texture;

		Uint32 brush_index = 0;
	};
}

namespace spintool::rom
{
	struct TileSet
	{
		ROMData rom_data;
		Uint32 compressed_size = 0;
		Uint32 uncompressed_size = 0;

		Uint16 num_tiles = 0;
		std::vector<Uint8> uncompressed_data;
		std::vector<rom::Tile> tiles;

		static TilesetEntry LoadFromROM(const SpinballROM& src_rom, Uint32 rom_offset, CompressionAlgorithm compression_algorithm);
		static TilesetEntry LoadFromROM_SSCCompression(const SpinballROM& src_rom, Uint32 rom_offset);
		static TilesetEntry LoadFromROM_LZSSCompression(const SpinballROM& src_rom, Uint32 rom_offset);
		std::shared_ptr<const Sprite> CreateSpriteFromTile(const Uint32 offset) const;
		std::shared_ptr<SpriteTile> CreateSpriteTileFromTile(const Uint32 tile_index) const;

		constexpr const static Uint16 s_tile_width = 0x08;
		constexpr const static Uint16 s_tile_height = 0x08;
		constexpr const static Uint16 s_tile_total_pixels = s_tile_width * s_tile_height;
		constexpr const static Uint16 s_tile_total_bytes = (s_tile_width / 2) * s_tile_height;
	};
}