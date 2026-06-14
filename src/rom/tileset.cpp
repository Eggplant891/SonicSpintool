#include "rom/tileset.h"

#include "rom/sprite.h"
#include "rom/spinball_rom.h"
#include "rom/ssc_decompressor.h"
#include "rom/lzss_decompressor.h"
#include "rom/tile.h"
#include "rom/ssc_compressor.h"

namespace spintool::rom
{
	TilesetEntry TileSet::LoadFromROM(const SpinballROM& src_rom, Uint32 rom_offset, CompressionAlgorithm compression_algorithm)
	{
		switch (compression_algorithm)
		{
			case CompressionAlgorithm::SSC:
				return LoadFromROM_SSCCompression(src_rom, rom_offset);

			case CompressionAlgorithm::LZSS:
				return LoadFromROM_LZSSCompression(src_rom, rom_offset);

			case CompressionAlgorithm::NONE:
			default:
				return TilesetEntry{};
		}
	}

	Ptr32 TileSet::SaveToROM_SSCCompression(SpinballROM& src_rom, Uint32 rom_offset) const
	{
		Ptr32 current_offset = rom_offset;
		current_offset = src_rom.WriteUint16(current_offset, num_tiles);
		const SSCCompressionResult compressed_data = rom::SSCCompressor::CompressData(uncompressed_data, 0, num_tiles * 64);
		for (size_t i = 0; i < compressed_data.size(); ++i)
		{
			current_offset = src_rom.WriteUint8(current_offset, compressed_data.at(i));
		}

		return current_offset;
	}

	TilesetEntry TileSet::LoadFromROM_SSCCompression(const SpinballROM& src_rom, Uint32 rom_offset)
	{
		constexpr Uint32 uncompressed_tile_size = TileSet::s_tile_total_bytes;
		auto new_tileset = std::make_unique<rom::TileSet>();

		new_tileset->uncompressed_data.clear();

		if (rom_offset > src_rom.m_buffer.size() || src_rom.m_buffer.size() - rom_offset < 2)
		{
			SSCDecompressionResult results;
			results.error_msg = "SSC tileset header is outside the ROM";
			return { std::move(new_tileset), results };
		}

		new_tileset->num_tiles = static_cast<Uint16>(
			(static_cast<Uint16>(src_rom.m_buffer[rom_offset]) << 8) |
			static_cast<Uint16>(src_rom.m_buffer[rom_offset + 1]));

		if (new_tileset->num_tiles == 0 || new_tileset->num_tiles > 0x1000)
		{
			SSCDecompressionResult results;
			results.error_msg = "Invalid SSC tile count";
			return { std::move(new_tileset), results };
		}

		SSCDecompressionResult results = rom::SSCDecompressor::DecompressData(
			src_rom.m_buffer, rom_offset + 2,
			static_cast<Uint32>(new_tileset->num_tiles) * s_tile_total_bytes);
		if (results.error_msg.has_value())
		{
			return { std::move(new_tileset), results };
		}

		new_tileset->uncompressed_size = static_cast<Uint32>(results.uncompressed_data.size());
		new_tileset->compressed_size = results.rom_data.real_size;
		new_tileset->uncompressed_data = std::move(results.uncompressed_data);

		for (size_t tile_index = 0; tile_index < new_tileset->num_tiles; ++tile_index)
		{
			const size_t relative_offset = (tile_index * s_tile_total_bytes);
			if (relative_offset >= new_tileset->uncompressed_data.size())
			{
				break;
			}

			rom::Tile new_tile;
			
			const Uint8* tile_start_byte = &new_tileset->uncompressed_data[relative_offset];
			const Uint8* current_byte = tile_start_byte;
			const size_t total_pixels = TileSet::s_tile_total_bytes;

			for (size_t i = 0; i < total_pixels && relative_offset + i < new_tileset->uncompressed_data.size(); i += 1)
			{
				const Uint32 left_byte = (0xF0 & *current_byte) >> 4;
				const Uint32 right_byte = 0x0F & *current_byte;

				new_tile.pixel_data.emplace_back(left_byte);
				new_tile.pixel_data.emplace_back(right_byte);

				++current_byte;
			}

			for (Uint8 x = 0; x < 4 && new_tile.is_x_symmetrical; ++x)
			{
				for (Uint8 y = 0; y < 8 && new_tile.is_x_symmetrical; ++y)
				{
					const Uint32 flip_lhs_index = (y * 8) + x;
					const Uint32 flip_rhs_index = (y * 8) + (7 - x);
					if (flip_lhs_index < new_tile.pixel_data.size() && flip_rhs_index < new_tile.pixel_data.size() && new_tile.pixel_data[flip_lhs_index] != new_tile.pixel_data[flip_rhs_index])
					{
						new_tile.is_x_symmetrical = false;
					}
				}
			}

			for (Uint8 x = 0; x < 8 && new_tile.is_y_symmetrical; ++x)
			{
				for (Uint8 y = 0; y < 4 && new_tile.is_y_symmetrical; ++y)
				{
					const Uint32 flip_lhs_index = (y * 8) + x;
					const Uint32 flip_rhs_index = ((7 - y) * 8) + x;
					if (flip_lhs_index < new_tile.pixel_data.size() && flip_rhs_index < new_tile.pixel_data.size() &&  new_tile.pixel_data[flip_lhs_index] != new_tile.pixel_data[flip_rhs_index])
					{
						new_tile.is_y_symmetrical = false;
					}
				}
			}

			new_tileset->tiles.emplace_back(std::move(new_tile));
		}

		new_tileset->rom_data.SetROMData(results.rom_data.rom_offset - 2, results.rom_data.rom_offset_end);

		return { std::move(new_tileset), results };
	}

	TilesetEntry TileSet::LoadFromROM_LZSSCompression(const SpinballROM& src_rom, Uint32 rom_offset)
	{
		auto new_tileset = std::make_unique<rom::TileSet>();

		//new_tileset->num_tiles = (static_cast<Sint16>(*(&src_rom.m_buffer[rom_offset])) << 8) | static_cast<Sint16>(*(&src_rom.m_buffer[rom_offset + 1]));
		new_tileset->uncompressed_data.clear();

		if (rom_offset >= src_rom.m_buffer.size())
		{
			LZSSDecompressionResult results;
			results.error_msg = "LZSS offset is outside the ROM";
			return { std::move(new_tileset), results };
		}

		// Use the portable implementation. The legacy implementation treats
		// original Mega Drive RAM/ROM addresses as std::vector indices.
		LZSSDecompressionResult results =
			rom::LZSSDecompressor::DecompressDataRefactored(src_rom.m_buffer, rom_offset);

		if (results.error_msg.has_value())
		{
			return { std::move(new_tileset), results };
		}

		new_tileset->uncompressed_size = static_cast<Uint32>(results.uncompressed_data.size());
		new_tileset->compressed_size = results.rom_data.real_size;
		new_tileset->uncompressed_data = std::move(results.uncompressed_data);
		// The format contains a two-byte prefix. Never erase beyond the buffer.
		if (new_tileset->uncompressed_data.size() < 2)
		{
			results.error_msg = "LZSS output is too small (missing two-byte prefix)";
			return { std::move(new_tileset), results };
		}
		new_tileset->uncompressed_data.erase(
			new_tileset->uncompressed_data.begin(),
			new_tileset->uncompressed_data.begin() + 2);
		new_tileset->num_tiles = static_cast<Uint16>(new_tileset->uncompressed_data.size()) / TileSet::s_tile_total_bytes;

		for (size_t tile_index = 0; tile_index < new_tileset->num_tiles; ++tile_index)
		{
			const size_t relative_offset = (tile_index * s_tile_total_bytes);
			rom::Tile new_tile;

			const Uint8* tile_start_byte = &new_tileset->uncompressed_data[relative_offset];
			const Uint8* current_byte = tile_start_byte;
			const size_t total_pixels = TileSet::s_tile_total_bytes;

			for (size_t i = 0; i < total_pixels && relative_offset + i < new_tileset->uncompressed_data.size(); i += 1)
			{
				const Uint32 left_byte = (0xF0 & *current_byte) >> 4;
				const Uint32 right_byte = 0x0F & *current_byte;

				new_tile.pixel_data.emplace_back(left_byte);
				new_tile.pixel_data.emplace_back(right_byte);

				++current_byte;
			}

			new_tileset->tiles.emplace_back(std::move(new_tile));
		}

		new_tileset->rom_data.SetROMData(results.rom_data.rom_offset, results.rom_data.rom_offset_end);


		return { std::move(new_tileset), results };
	}

	std::shared_ptr<rom::SpriteTile> TileSet::CreateSpriteTileFromTile(const Uint32 tile_index) const
	{
		const Uint32 relative_offset = tile_index * s_tile_total_bytes;

		if (uncompressed_data.empty() ||
			relative_offset > uncompressed_data.size() ||
			uncompressed_data.size() - relative_offset < s_tile_total_bytes)
		{
			return nullptr;
		}

		std::shared_ptr<rom::SpriteTile> sprite_tile = std::make_shared<rom::SpriteTile>();
		sprite_tile->x_size = s_tile_width;
		sprite_tile->y_size = s_tile_height;

		const int num_tiles_per_row = 16;
		const int tile_spacing = 0;
		Uint32 current_x_offset = 0;
		Uint32 current_y_offset = 0;


		if (tile_index != 0)
		{
			const Uint32 x_offset = tile_index % num_tiles_per_row;
			if (x_offset == 0)
			{
				current_x_offset = 0;
				current_y_offset = (s_tile_height + tile_spacing) * (tile_index / num_tiles_per_row);

			}
			else
			{
				current_x_offset = (s_tile_width * x_offset) + tile_spacing;
				current_y_offset = (s_tile_height + tile_spacing) * ((tile_index- x_offset) / num_tiles_per_row);
			}
		}

		sprite_tile->x_offset = static_cast<Sint16>(current_x_offset);
		sprite_tile->y_offset = static_cast<Sint16>(current_y_offset);

		const size_t total_pixels = sprite_tile->x_size * sprite_tile->y_size;

		const Uint8* tile_start_byte = &uncompressed_data[relative_offset];
		const Uint8* current_byte = tile_start_byte;

		for (size_t i = 0; i < s_tile_total_bytes; ++i)
		{
			const Uint32 left_byte = (0xF0 & *current_byte) >> 4;
			const Uint32 right_byte = 0x0F & *current_byte;

			sprite_tile->pixel_data.emplace_back(left_byte);
			sprite_tile->pixel_data.emplace_back(right_byte);

			++current_byte;
		}

		sprite_tile->tile_rom_data.SetROMData(rom_data.rom_offset + relative_offset, rom_data.rom_offset + relative_offset + static_cast<Uint32>(current_byte - tile_start_byte));

		return sprite_tile;
	}

	std::shared_ptr<const Sprite> TileSet::CreateSpriteFromTile(const Uint32 relative_offset) const
	{
		if (relative_offset > uncompressed_data.size() ||
			uncompressed_data.size() - relative_offset < s_tile_total_bytes)
		{
			return nullptr;
		}

		std::shared_ptr<rom::Sprite> new_sprite = std::make_shared<rom::Sprite>();

		const Uint8* tileset_start_byte = &uncompressed_data[relative_offset];
		const Uint8* current_byte = tileset_start_byte;
		const Uint32 available_tiles = static_cast<Uint32>(uncompressed_data.size() / s_tile_total_bytes);
		const Uint32 num_tiles_to_wrangle = std::min<Uint32>(num_tiles, available_tiles);

		new_sprite->rom_data.rom_offset = rom_data.rom_offset;
		new_sprite->num_tiles = static_cast<Uint16>(num_tiles_to_wrangle);

		for (Uint32 i = 0; i < num_tiles_to_wrangle; ++i)
		{
			auto tile = TileSet::CreateSpriteTileFromTile(i);
			if (!tile)
			{
				break;
			}
			current_byte += tile->tile_rom_data.real_size;
			new_sprite->sprite_tiles.emplace_back(std::move(tile));
		}
		new_sprite->rom_data.SetROMData(rom_data.rom_offset + relative_offset, rom_data.rom_offset + static_cast<Uint32>(current_byte - &uncompressed_data[relative_offset]));

		return new_sprite;
	}

	SDLSurfaceHandle TileSet::RenderToSurface(const rom::Palette& palette_line) const
	{
		SDLSurfaceHandle out_surface;

		constexpr int picker_width = 20;
		int max_x_size = 0;
		int max_y_size = 0;


		Uint32 offset = 0;
		std::vector<std::shared_ptr<rom::SpriteTile>> tiles;

		for (Uint16 i = 0; i < num_tiles; ++i)
		{
			std::shared_ptr<rom::SpriteTile> sprite_tile = CreateSpriteTileFromTile(i);

			if (sprite_tile == nullptr)
			{
				break;
			}

			const size_t current_brush_offset = i;

			sprite_tile->x_offset = static_cast<Sint16>(current_brush_offset % picker_width) * rom::TileSet::s_tile_width;
			sprite_tile->y_offset = static_cast<Sint16>((current_brush_offset - (current_brush_offset % picker_width)) / picker_width) * rom::TileSet::s_tile_height;

			max_x_size = std::max(max_x_size, sprite_tile->x_offset + rom::TileSet::s_tile_width);
			max_y_size = std::max(max_y_size, sprite_tile->y_offset + rom::TileSet::s_tile_height);

			sprite_tile->blit_settings.flip_horizontal = false;
			sprite_tile->blit_settings.flip_vertical = false;

			sprite_tile->blit_settings.palette = std::make_shared<rom::Palette>(palette_line);
			tiles.emplace_back(std::move(sprite_tile));
		}

		if (tiles.empty() || max_x_size <= 0 || max_y_size <= 0)
		{
			return {};
		}

		out_surface = SDLSurfaceHandle{ SDL_CreateSurface(max_x_size, max_y_size, SDL_PIXELFORMAT_RGBA32) };
		if (!out_surface)
		{
			return {};
		}
		const SDL_PixelFormatDetails* format_details = SDL_GetPixelFormatDetails(out_surface->format);
		if (!format_details ||
			!SDL_SetSurfaceColorKey(out_surface.get(), true, SDL_MapRGBA(format_details, nullptr, 0, 0, 0, 0)) ||
			!SDL_ClearSurface(out_surface.get(), 0.0f, 0, 0, 0))
		{
			return {};
		}

		BoundingBox picker_bbox{ 0, 0, out_surface->w, out_surface->h };

		for (std::shared_ptr<rom::SpriteTile>& sprite_tile : tiles)
		{
			sprite_tile->BlitPixelDataToSurface(out_surface.get(), picker_bbox, sprite_tile->pixel_data);
		}

		return out_surface;
	}

}
