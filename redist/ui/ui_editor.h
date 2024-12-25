#pragma once

#include "render.h"
#include "types/sdl_handle_defs.h"
#include "rom/spinball_rom.h"
#include "ui/ui_sprite.h"
#include "ui/ui_palette.h"

#include "ui/ui_sprite_viewer.h"
#include "ui/ui_sprite_navigator.h"
#include "ui/ui_tileset_navigator.h"
#include "ui/ui_palette_viewer.h"
#include "ui/ui_sprite_importer.h"

#include "imgui.h"
#include <vector>
#include <mutex>
#include <filesystem>

namespace spintool
{
	class EditorUI
	{
	public:
		EditorUI();

		void Initialise();
		bool AttemptLoadROM(const std::filesystem::path& rom_path);
		void Update();
		void Shutdown();

		bool IsROMLoaded() const;
		rom::SpinballROM& GetROM();
		std::filesystem::path GetROMLoadPath() const;
		std::filesystem::path GetROMExportPath() const;
		std::filesystem::path GetSpriteExportPath() const;

		const std::vector<std::shared_ptr<const rom::TileSet>>& GetTilesets() const;
		const std::vector<std::shared_ptr<rom::Palette>>& GetPalettes() const;
		void OpenSpriteViewer(std::shared_ptr<const rom::Sprite>& sprite);
		void OpenSpriteImporter(int rom_offset);
		std::recursive_mutex m_render_to_texture_mutex;

	private:
		rom::SpinballROM m_rom;

		std::filesystem::path m_rom_path;
		std::filesystem::path m_rom_load_path;
		std::filesystem::path m_rom_export_path;
		std::filesystem::path m_sprite_export_path;

		std::vector<std::shared_ptr<rom::Palette>> m_palettes;

		std::vector<std::unique_ptr<EditorSpriteViewer>> m_sprite_viewer_windows;
		EditorSpriteNavigator m_sprite_navigator;
		EditorTilesetNavigator m_tileset_navigator;
		EditorPaletteViewer m_palette_viewer;
		EditorSpriteImporter m_sprite_importer;

		bool m_change_path_popup_open = false;
	};
}