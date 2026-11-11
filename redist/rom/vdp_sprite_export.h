#pragma once

#include "SDL3/SDL_stdinc.h"

#include <filesystem>
#include <string>
#include <vector>

namespace spintool::rom
{
	class SpinballROM;
}

namespace spintool::rom
{
	// Exports the game's sprite pieces in the order the VDP stores them, so the
	// game can send each piece to VRAM with one DMA instead of reshuffling it
	// with the CPU.
	//
	// In the ROM, a piece's 4bpp pixels are stored row by row. When the game
	// uploads a piece (VDP_SendSpriteData), it sends it one 8-pixel column at a
	// time, a row at a time, pads each column with zero rows to a whole tile,
	// and pads a trailing 4-pixel half column with a zero word. The exported
	// block for a piece is exactly the bytes that routine writes to VRAM.

	struct VDPSpritePiece
	{
		Uint32 pixel_data_offset = 0;  // ROM address of the piece's pixel data
		Uint32 header_offset = 0;      // ROM address of the piece's height and width bytes
		Uint8 height = 0;              // rows
		Uint8 width_bytes = 0;         // bytes per row (2 pixels per byte)
		Uint32 block_offset = 0;       // offset of the converted block in the exported data
		Uint32 block_size = 0;         // bytes; always a multiple of 32
	};

	struct VDPSpriteExport
	{
		std::vector<VDPSpritePiece> pieces;  // sorted by pixel_data_offset, then height and width
		std::vector<Uint8> data;             // the converted blocks
		Uint32 num_sprites = 0;
		Uint32 num_duplicate_blocks = 0;     // pieces whose block matched an earlier one
		std::vector<std::string> warnings;
	};

	struct VDPSpriteTable
	{
		Uint32 offset = 0;
		const char* name = "";
	};

	// The sprite tables the game uploads from (USA ROM addresses)
	[[nodiscard]] std::vector<VDPSpriteTable> GetDefaultVDPSpriteTables();

	// The bytes VDP_SendSpriteData writes to VRAM for one piece
	[[nodiscard]] std::vector<Uint8> ConvertSpritePieceToVDP(const SpinballROM& rom, Uint32 pixel_data_offset, Uint8 height, Uint8 width_bytes);

	[[nodiscard]] VDPSpriteExport BuildVDPSpriteExport(const SpinballROM& rom, const std::vector<VDPSpriteTable>& tables);

	// Writes <out_dir>/vdp_sprites.bin and <out_dir>/vdp_sprites.asm. The .asm
	// includes the .bin through incbin_path, which is relative to where the
	// assembler runs.
	bool WriteVDPSpriteExport(const VDPSpriteExport& sprite_export, const std::filesystem::path& out_dir, const std::string& incbin_path, const std::string& source_name, std::string& out_error);

	// Command line entry point: spintool --export-vdp-sprites <rom> <out dir> [--incbin-path <path>]
	int RunVDPSpriteExportCommand(int argc, char* argv[]);
}
