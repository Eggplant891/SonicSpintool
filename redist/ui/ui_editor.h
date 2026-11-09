#pragma once

#include "render.h"

#include "rom/spinball_rom.h"
#include "rom/metadata/rom_metadata.h"
#include "editor/editor_project.h"

#include "ui/ui_sprite_viewer.h"
#include "ui/ui_sprite_navigator.h"
#include "ui/ui_tileset_navigator.h"
#include "ui/ui_tile_layout_viewer.h"
#include "ui/ui_palette_viewer.h"
#include "ui/ui_sprite_importer.h"
#include "ui/ui_animation_navigator.h"
#include "ui/ui_game_object_metadata.h"

#include <filesystem>
#include <memory>
#include <mutex>
#include <vector>

namespace spintool
{
	class EditorUI
	{
	public:
		EditorUI();

		void SaveROMConfig() const;
		void LoadROMConfig();

		void SaveUIConfig() const;
		void LoadUIConfig();

		void Initialise();
		bool AttemptLoadROM(const std::filesystem::path& rom_path);
		void Update();
		void Shutdown();

		[[nodiscard]] bool IsROMLoaded() const;

		rom::SpinballROM& GetROM();
		[[nodiscard]] metadata::ROMMetadata* GetCurrentROMMetadata();
		void SaveGameObjectMetadata();
		void SaveAnimationMetadata();

		static std::filesystem::path GetROMLoadPath();
		static std::filesystem::path GetROMExportPath();
		static std::filesystem::path GetSpriteExportPath();
		static std::filesystem::path GetProjectsPath();
		static std::filesystem::path GetMetadataPath();
		static std::filesystem::path GetConfigPath();

		// Immutable source ROM selected from <project>/roms.
		[[nodiscard]] std::filesystem::path GetReferenceROMPath() const;

		// Editable copy actually loaded from <project>/rom_export.
		[[nodiscard]] std::filesystem::path GetWorkingROMPath() const;

		[[nodiscard]] const std::vector<TilesetEntry>& GetTilesets() const;
		[[nodiscard]] const std::vector<std::shared_ptr<rom::Palette>>& GetPalettes() const;
		void NotifyPaletteChanged();

		void OpenSpriteViewer(std::shared_ptr<const rom::Sprite>& sprite);
		void OpenImageImporter(rom::Sprite& sprite);
		void OpenImageImporter(
			rom::TileSet& tileset,
			const rom::PaletteSet& available_palettes
		);

		std::recursive_mutex m_render_to_texture_mutex;

	private:
		enum class ProjectNamePopupMode
		{
			NONE,
			RENAME,
			SAVE_AS
		};

		void DrawProjectNamePopup(bool open_popup);

		rom::SpinballROM m_rom;
		std::unique_ptr<Project> m_current_project;

		ProjectNamePopupMode m_project_name_popup_mode = ProjectNamePopupMode::NONE;
		char m_project_name_buffer[128] = {};
		std::string m_project_name_error;

		metadata::ROMMetadata* m_current_rom_metadata = nullptr;
		metadata::Metadata m_metadata;

		static std::filesystem::path s_rom_load_path;
		static std::filesystem::path s_rom_export_path;
		static std::filesystem::path s_sprite_export_path;
		static std::filesystem::path s_projects_path;
		static std::filesystem::path s_metadata_path;
		static std::filesystem::path s_config_path;

		std::filesystem::path m_reference_rom_path;
		std::filesystem::path m_working_rom_path;

		std::vector<std::shared_ptr<rom::Palette>> m_palettes;
		std::vector<std::unique_ptr<EditorSpriteViewer>> m_sprite_viewer_windows;

		EditorSpriteNavigator m_sprite_navigator;
		EditorTilesetNavigator m_tileset_navigator;
		EditorTileLayoutViewer m_tile_layout_viewer;
		EditorAnimationNavigator m_animation_navigator;
		EditorPaletteViewer m_palette_viewer;
		EditorGameObjectMetadataWindow m_game_object_metadata_window;
		EditorImageImporter m_sprite_importer;

		bool m_change_path_popup_open = false;
		float m_font_scale = 1.0f;
	};
}
