#include "rom/vdp_sprite_export.h"

#include "rom/spinball_rom.h"
#include "rom/sprite.h"
#include "rom/sprite_tile.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <tuple>

namespace spintool::rom
{
	namespace
	{
		std::string Hex(Uint32 value, int digits)
		{
			char text[16]{};
			snprintf(text, sizeof(text), "$%0*X", digits, static_cast<unsigned int>(value));
			return text;
		}

		Uint8 ReadByteOrZero(const SpinballROM& rom, Uint32 offset)
		{
			return offset < rom.m_buffer.size() ? rom.m_buffer[offset] : 0;
		}
	}

	std::vector<VDPSpriteTable> GetDefaultVDPSpriteTables()
	{
		return {
			{ 0x0013DC, "player" },
			{ 0x00E47A, "global" },
			{ 0x012B0C, "toxic_caves" },
			{ 0x020666, "lava_powerhouse" },
			{ 0x02B7A4, "the_machine" },
			{ 0x0318EE, "showdown" },
		};
	}

	std::vector<Uint8> ConvertSpritePieceToVDP(const SpinballROM& rom, const Uint32 pixel_data_offset, const Uint8 height, const Uint8 width_bytes)
	{
		// Mirrors VDP_SendSpriteData. Each column is 4 bytes (8 pixels) wide. In
		// the last column, a width that is not a multiple of 4 sends one word
		// of pixels and one zero word per row.
		std::vector<Uint8> out;
		if (height == 0 || width_bytes == 0)
		{
			return out;
		}

		const Uint32 num_columns = (static_cast<Uint32>(width_bytes) + 3U) / 4U;
		const bool partial_last_column = (width_bytes & 3U) != 0;
		const Uint32 pad_rows = (height & 7U) != 0 ? 8U - (height & 7U) : 0U;
		out.reserve(num_columns * (height + pad_rows) * 4U);

		for (Uint32 column = 0; column < num_columns; ++column)
		{
			const bool is_last = column + 1 == num_columns;
			for (Uint32 row = 0; row < height; ++row)
			{
				const Uint32 src = pixel_data_offset + (column * 4U) + (row * width_bytes);
				if (is_last && partial_last_column)
				{
					out.push_back(ReadByteOrZero(rom, src));
					out.push_back(ReadByteOrZero(rom, src + 1));
					out.push_back(0);
					out.push_back(0);
				}
				else
				{
					for (Uint32 i = 0; i < 4; ++i)
					{
						out.push_back(ReadByteOrZero(rom, src + i));
					}
				}
			}
			out.insert(out.end(), pad_rows * 4U, 0);
		}

		return out;
	}

	VDPSpriteExport BuildVDPSpriteExport(const SpinballROM& rom, const std::vector<VDPSpriteTable>& tables)
	{
		VDPSpriteExport result;

		// Each piece is identified by its pixel data address plus its size: the
		// game looks the converted block up by those when it queues an upload.
		std::map<std::tuple<Uint32, Uint8, Uint8>, VDPSpritePiece> pieces;
		std::set<Uint32> sprites_seen;
		std::map<std::vector<Uint8>, Uint32> blocks;

		for (const VDPSpriteTable& table : tables)
		{
			if (table.offset + 4 > rom.m_buffer.size())
			{
				result.warnings.emplace_back(std::string("sprite table ") + table.name + " is outside the ROM");
				continue;
			}

			const Uint16 num_sprites = rom.ReadUint16(table.offset);
			for (Uint16 i = 0; i < num_sprites; ++i)
			{
				const Uint32 sprite_offset = table.offset + rom.ReadUint16(table.offset + 4 + (i * 2U));
				if (sprites_seen.insert(sprite_offset).second == false)
				{
					continue;
				}

				const std::shared_ptr<const Sprite> sprite = Sprite::LoadFromROM(rom, sprite_offset);
				if (!sprite)
				{
					result.warnings.emplace_back(std::string("sprite table ") + table.name + " entry " + std::to_string(i) + " at " + Hex(sprite_offset, 6) + " is not a valid sprite");
					continue;
				}
				++result.num_sprites;

				for (const auto& tile : sprite->sprite_tiles)
				{
					VDPSpritePiece piece;
					piece.pixel_data_offset = tile->tile_rom_data.rom_offset;
					piece.header_offset = tile->header_rom_data.rom_offset + 4;
					piece.height = tile->y_size;
					piece.width_bytes = static_cast<Uint8>(tile->x_size / 2);

					const auto key = std::make_tuple(piece.pixel_data_offset, piece.height, piece.width_bytes);
					if (pieces.count(key) != 0)
					{
						continue;
					}

					std::vector<Uint8> block = ConvertSpritePieceToVDP(rom, piece.pixel_data_offset, piece.height, piece.width_bytes);
					piece.block_size = static_cast<Uint32>(block.size());

					const auto existing = blocks.find(block);
					if (existing != blocks.end())
					{
						piece.block_offset = existing->second;
						++result.num_duplicate_blocks;
					}
					else
					{
						piece.block_offset = static_cast<Uint32>(result.data.size());
						result.data.insert(result.data.end(), block.begin(), block.end());
						blocks.emplace(std::move(block), piece.block_offset);
					}

					pieces.emplace(key, piece);
				}
			}
		}

		result.pieces.reserve(pieces.size());
		for (auto& [key, piece] : pieces)
		{
			result.pieces.push_back(piece);
		}

		return result;
	}

	bool WriteVDPSpriteExport(const VDPSpriteExport& sprite_export, const std::filesystem::path& out_dir, const std::string& incbin_path, const std::string& source_name, std::string& out_error)
	{
		std::error_code ec;
		std::filesystem::create_directories(out_dir, ec);

		const std::filesystem::path bin_path = out_dir / "vdp_sprites.bin";
		const std::filesystem::path asm_path = out_dir / "vdp_sprites.asm";

		{
			std::ofstream bin(bin_path, std::ios::binary);
			if (!bin)
			{
				out_error = "could not write " + bin_path.string();
				return false;
			}
			bin.write(reinterpret_cast<const char*>(sprite_export.data.data()), static_cast<std::streamsize>(sprite_export.data.size()));
		}

		std::ofstream out(asm_path);
		if (!out)
		{
			out_error = "could not write " + asm_path.string();
			return false;
		}

		out << "; ---------------------------------------------------------------------------\n";
		out << "; Sprite pieces in VDP tile order. Generated by spintool --export-vdp-sprites\n";
		out << "; from " << source_name << ": do not edit.\n";
		out << ";\n";
		out << "; Each block is exactly what VDP_SendSpriteData writes to VRAM for the piece,\n";
		out << "; so it can be sent with one DMA. The lookup is sorted by pixel data address,\n";
		out << "; then height and width. Each entry is VDPSPRITE_ENTRY_SIZE bytes:\n";
		out << ";   dc.l  pixel data address in the ROM (the upload's source pointer)\n";
		out << ";   dc.b  height in rows, width in bytes (as in the piece header)\n";
		out << ";   dc.w  block length in words (the DMA length)\n";
		out << ";   dc.l  address of the converted block\n";
		out << "; VDPSprites_PageIndex narrows a lookup to one 256-byte page of addresses.\n";
		out << ";\n";
		out << "; " << sprite_export.num_sprites << " sprites, " << sprite_export.pieces.size() << " pieces, "
			<< sprite_export.data.size() << " bytes of blocks (" << sprite_export.num_duplicate_blocks << " pieces share a block)\n";
		out << "; ---------------------------------------------------------------------------\n\n";
		// Page index: for each 256-byte page of pixel data addresses, the
		// first lookup entry at or after it, plus one final entry, so a lookup
		// only scans the entries between Index[page] and Index[page + 1].
		const Uint32 last_pixel_offset = sprite_export.pieces.empty() ? 0 : sprite_export.pieces.back().pixel_data_offset;
		const Uint32 num_pages = (last_pixel_offset >> 8) + 1;
		std::vector<Uint16> page_index(num_pages + 1);
		{
			size_t entry = 0;
			for (Uint32 page = 0; page <= num_pages; ++page)
			{
				while (entry < sprite_export.pieces.size() && sprite_export.pieces[entry].pixel_data_offset < (page << 8))
				{
					++entry;
				}
				page_index[page] = static_cast<Uint16>(entry);
			}
		}

		// equ, not =: the game code refers to these before this file is included,
		// and asm68k warns about forward references to redefinable (=) symbols
		out << "VDPSPRITE_ENTRY_SIZE    equ 12\n";
		out << "VDPSprites_Count        equ " << sprite_export.pieces.size() << "\n";
		out << "VDPSprites_NumPages     equ " << num_pages << "         ; pages of 256 bytes of pixel data addresses\n\n";
		out << "                even\n";
		out << "VDPSprites_Lookup:\n";
		for (const VDPSpritePiece& piece : sprite_export.pieces)
		{
			out << "                dc.l    " << Hex(piece.pixel_data_offset, 6)
				<< "\n                dc.b    " << Hex(piece.height, 2) << "," << Hex(piece.width_bytes, 2)
				<< "\n                dc.w    " << Hex(piece.block_size / 2, 4)
				<< "\n                dc.l    VDPSprites_Data+" << Hex(piece.block_offset, 6) << "\n";
		}
		out << "VDPSprites_LookupEnd:\n\n";
		out << "; First lookup entry for each 256-byte page of pixel data addresses\n";
		out << "; (VDPSprites_NumPages + 1 words)\n";
		out << "VDPSprites_PageIndex:\n";
		for (size_t i = 0; i < page_index.size(); i += 8)
		{
			out << "                dc.w    ";
			for (size_t j = i; j < std::min(i + 8, page_index.size()); ++j)
			{
				out << (j != i ? "," : "") << Hex(page_index[j], 4);
			}
			out << "\n";
		}
		out << "\n";
		out << "                even\n";
		out << "VDPSprites_Data:\n";
		out << "                incbin  \"" << incbin_path << "\"\n";
		out << "VDPSprites_DataEnd:\n";

		return true;
	}

	int RunVDPSpriteExportCommand(int argc, char* argv[])
	{
		// argv[1] is --export-vdp-sprites
		if (argc < 4)
		{
			std::cerr << "usage: spintool --export-vdp-sprites <rom> <out dir> [--incbin-path <path>]\n";
			return 2;
		}

		const std::filesystem::path rom_path = argv[2];
		const std::filesystem::path out_dir = argv[3];
		std::string incbin_path = (out_dir / "vdp_sprites.bin").generic_string();
		for (int i = 4; i + 1 < argc; i += 2)
		{
			const std::string option = argv[i];
			if (option == "--incbin-path")
			{
				incbin_path = argv[i + 1];
			}
			else
			{
				std::cerr << "unknown option " << option << '\n';
				return 2;
			}
		}

		SpinballROM rom;
		if (!rom.LoadROMFromPath(rom_path))
		{
			std::cerr << "could not load " << rom_path.string() << '\n';
			return 1;
		}

		const VDPSpriteExport sprite_export = BuildVDPSpriteExport(rom, GetDefaultVDPSpriteTables());
		for (const std::string& warning : sprite_export.warnings)
		{
			std::cerr << "warning: " << warning << '\n';
		}

		std::string error;
		if (!WriteVDPSpriteExport(sprite_export, out_dir, incbin_path, rom_path.filename().string(), error))
		{
			std::cerr << error << '\n';
			return 1;
		}

		std::cout << sprite_export.num_sprites << " sprites, " << sprite_export.pieces.size() << " pieces, "
			<< sprite_export.data.size() << " bytes written to " << out_dir.string() << '\n';
		return 0;
	}
}
