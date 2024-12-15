#pragma once

#include "render.h"
#include "sdl_handle_defs.h"
#include "spinball_rom.h"
#include "ui_sprite.h"
#include "ui_palette.h"

#include "ui_sprite_viewer.h"
#include "ui_sprite_navigator.h"
#include "ui_palette_viewer.h"
#include "ui_sprite_importer.h"

#include "imgui.h"
#include <vector>
#include <mutex>

namespace spintool
{
	class EditorUI
	{
	public:
		EditorUI();

		void Initialise();
		void Update();
		void Shutdown();

		SpinballROM& GetROM();
		const std::vector<VDPPalette>& GetPalettes() const;
		void OpenSpriteViewer(std::shared_ptr<const SpinballSprite>& sprite);
		std::recursive_mutex m_render_to_texture_mutex;

	private:
		char m_rom_path[4096] = "E:/Development/_RETRODEV/MegaDrive/Spinball/SpinballDisassembly/bin/original/Sonic The Hedgehog Spinball (USA).md";
		SpinballROM m_rom;
		std::vector<VDPPalette> m_palettes;

		std::vector<std::unique_ptr<EditorSpriteViewer>> m_sprite_viewer_windows;
		EditorSpriteNavigator m_sprite_navigator;
		EditorPaletteViewer m_palette_viewer;
		EditorSpriteImporter m_sprite_importer;

		bool m_change_path_popup_open = false;
	};
}