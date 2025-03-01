#pragma once

#include "types/sdl_handle_defs.h"
#include "types/bounding_box.h"
#include "render.h"
#include "rom/tileset.h"
#include "rom/sprite.h"
#include "rom/palette.h"

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
		std::vector<std::shared_ptr<rom::Palette>> LoadPalettes(size_t num_palettes) const;

		void SaveROM();

		void RenderToSurface(SDL_Surface* surface, size_t offset, Point dimensions) const;
		void RenderToSurface(SDL_Surface* surface, size_t offset, Point dimensions, const rom::Palette& palette) const;
		void BlitRawPixelDataToSurface(SDL_Surface* surface, const BoundingBox& bounds, const std::vector<Uint32>& pixels_data) const;

		Uint8 ReadUint8(size_t offset) const;
		Uint16 ReadUint16(size_t offset) const;
		Uint32 ReadUint32(size_t offset) const;

		size_t WriteUint8(size_t offset, Uint8 value);
		size_t WriteUint16(size_t offset, Uint16 value);
		size_t WriteUint32(size_t offset, Uint32 value);

		std::vector<unsigned char> m_buffer;
		std::filesystem::path m_filepath;
		std::vector<std::shared_ptr<rom::Palette>> m_palettes;

		// Hardcoded resources
		const std::vector<std::shared_ptr<spintool::rom::Palette>>& GetGlobalPalettes() const;
		std::shared_ptr<rom::PaletteSet> GetOptionsScreenPaletteSet() const;
		std::shared_ptr<rom::PaletteSet> GetIntroCutscenePaletteSet() const;
		std::shared_ptr<rom::PaletteSet> GetMainMenuPaletteSet() const;
		std::shared_ptr<rom::PaletteSet> GetSegaLogoIntroPaletteSet() const;
	};
}