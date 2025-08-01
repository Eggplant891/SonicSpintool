#pragma once

#include "ui/ui_editor_window.h"
#include "ui/ui_palette.h"
#include "rom/palette.h"

#include "rom/spinball_rom.h"
#include "types/sdl_handle_defs.h"
#include "types/rom_ptr.h"

#include "imgui.h"

#include <string>
#include <vector>
#include <memory>
#include <variant>

namespace spintool
{
	class EditorUI;

	namespace rom
	{
		struct Sprite;
		struct TileSet;
	}
}

namespace spintool
{
	struct ColourPaletteMapping
	{
		ImColor colour;
		Uint8 palette_index;
	};

	class EditorImageImporter : public EditorWindowBase
	{
	public:
		using EditorWindowBase::EditorWindowBase;

		void Update() override;
		void SetTarget(rom::Sprite& target_sprite);
		void SetTarget(rom::TileSet& target_tileset);
		void SetAvailablePalettes(const std::vector<std::shared_ptr<rom::Palette>>& palette_lines);
		void SetAvailablePalettes(const rom::PaletteSetArray& palette_set_lines);

	private:
		void InnerUpdate();
		void RenderTileset(rom::TileSet& tileset);
		void DrawTileSetImport();
		void DrawSpriteImport();
		SDLSurfaceHandle m_imported_image;
		SDLSurfaceHandle m_preview_image;
		SDLSurfaceHandle m_export_preview_image;
		SDLTextureHandle m_rendered_imported_image;
		SDLTextureHandle m_rendered_preview_image;
		SDLTextureHandle m_export_preview_texture;

		std::vector<ColourPaletteMapping> m_detected_colours;
		int m_selected_palette_index = 0;
		std::variant<std::unique_ptr<rom::TileSet>, std::shared_ptr<const rom::Sprite>> m_result_asset;
		std::variant<rom::TileSet*, rom::Sprite*> m_target_asset;
		std::vector<std::shared_ptr<rom::Palette>> m_available_palettes;
		rom::Palette m_selected_palette;
		SDLPaletteHandle m_preview_palette;
		std::string m_loaded_path;

		bool m_force_update_write_location = false;
		bool m_append_existing = true;
	};
}