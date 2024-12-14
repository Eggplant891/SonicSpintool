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
		void OpenSpriteViewer(const std::shared_ptr<SpinballSprite>& sprite, const std::vector<UIPalette>& palettes);
		std::recursive_mutex m_render_to_texture_mutex;

	private:
		SpinballROM m_rom;

		std::vector<EditorSpriteViewer> m_sprite_viewer_windows;
		EditorSpriteNavigator m_sprite_navigator;
		EditorPaletteViewer m_palette_viewer;
		EditorSpriteImporter m_sprite_importer;
	};
}