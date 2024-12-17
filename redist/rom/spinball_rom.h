#pragma once

#include "types/sdl_handle_defs.h"
#include "types/bounding_box.h"
#include "render.h"
#include "rom/tileset.h"
#include "rom/sprite.h"

#include "SDL3/SDL.h"
#include "imgui.h"

#include <vector>
#include <memory>
#include <optional>
#include <filesystem>

namespace spintool::rom
{
	class SpinballROM
	{
	public:
		void LoadTileData(size_t rom_offset);
		bool LoadROMFromPath(const std::filesystem::path& path);
		size_t GetOffsetForNextSprite(const rom::Sprite& current_sprite) const;
		std::shared_ptr<const rom::Sprite> LoadSprite(const size_t offset, bool packed_data_mode, bool try_to_read_missed_data);
		std::shared_ptr<const rom::Sprite> LoadLevelTile(const std::vector<unsigned char>& data_source, const size_t offset);
		std::vector<rom::Palette> LoadPalettes(size_t num_palettes);

		void SaveROM();

		void RenderToSurface(SDL_Surface* surface, size_t offset, Point dimensions) const;
		void BlitRawPixelDataToSurface(SDL_Surface* surface, const BoundingBox& bounds, const std::vector<Uint32>& pixels_data) const;

		std::vector<unsigned char> m_buffer;
		rom::TileSet m_toxic_caves_bg_data;
		std::filesystem::path m_filepath;
	};
}