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
		bool LoadROMFromPath(const std::filesystem::path& path);
		size_t GetOffsetForNextSprite(const rom::Sprite& current_sprite) const;
		std::vector<std::shared_ptr<rom::Palette>> LoadPalettes(size_t num_palettes);

		void SaveROM();

		void RenderToSurface(SDL_Surface* surface, size_t offset, Point dimensions) const;
		void RenderToSurface(SDL_Surface* surface, size_t offset, Point dimensions, const rom::Palette& palette) const;
		void BlitRawPixelDataToSurface(SDL_Surface* surface, const BoundingBox& bounds, const std::vector<Uint32>& pixels_data) const;

		std::vector<unsigned char> m_buffer;
		std::filesystem::path m_filepath;
	};
}