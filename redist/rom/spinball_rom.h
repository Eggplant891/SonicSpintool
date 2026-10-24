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
		[[nodiscard]] Uint32 GetOffsetForNextSprite(const rom::Sprite& current_sprite) const;
		[[nodiscard]] std::vector<std::shared_ptr<rom::Palette>> LoadPalettes(Uint32 num_palettes) const;

		void SaveROM();

		void RenderToSurface(SDL_Surface* surface, Uint32 offset, Point dimensions) const;
		void RenderToSurface(SDL_Surface* surface, Uint32 offset, Point dimensions, const rom::Palette& palette) const;
		void BlitRawPixelDataToSurface(SDL_Surface* surface, const BoundingBox& bounds, const std::vector<Uint32>& pixels_data) const;

		[[nodiscard]] Uint8 ReadUint8(Uint32 offset) const;
		[[nodiscard]] Uint16 ReadUint16(Uint32 offset) const;
		[[nodiscard]] Uint32 ReadUint32(Uint32 offset) const;

		[[nodiscard]] Sint8 ReadSint8(Uint32 offset) const;
		[[nodiscard]] Sint16 ReadSint16(Uint32 offset) const;
		[[nodiscard]] Sint32 ReadSint32(Uint32 offset) const;

		Uint32 ReadUint8(Uint32 offset, Uint8& out_value) const;
		Uint32 ReadUint16(Uint32 offset, Uint16& out_value) const;
		Uint32 ReadUint32(Uint32 offset, Uint32& out_value) const;

		Uint32 ReadSint8(Uint32 offset, Sint8& out_value) const;
		Uint32 ReadSint16(Uint32 offset, Sint16& out_value) const;
		Uint32 ReadSint32(Uint32 offset, Sint32& out_value) const;

		Uint32 WriteUint8(Uint32 offset, Uint8 value);
		Uint32 WriteUint16(Uint32 offset, Uint16 value);
		Uint32 WriteUint32(Uint32 offset, Uint32 value);

		Uint32 WriteSint8(Uint32 offset, Sint8 value);
		Uint32 WriteSint16(Uint32 offset, Sint16 value);
		Uint32 WriteSint32(Uint32 offset, Sint32 value);

		std::vector<unsigned char> m_buffer;
		std::filesystem::path m_filepath;
		std::vector<std::shared_ptr<rom::Palette>> m_palettes;

		// Hardcoded resources
		[[nodiscard]] const std::vector<std::shared_ptr<spintool::rom::Palette>>& GetGlobalPalettes() const;
		[[nodiscard]] std::shared_ptr<rom::PaletteSet> GetOptionsScreenPaletteSet() const;
		[[nodiscard]] std::shared_ptr<rom::PaletteSet> GetIntroCutscenePaletteSet() const;
		[[nodiscard]] std::shared_ptr<rom::PaletteSet> GetMainMenuPaletteSet() const;
		[[nodiscard]] std::shared_ptr<rom::PaletteSet> GetSegaLogoIntroPaletteSet() const;
	};
}