#pragma once

#include "sdl_handle_defs.h"
#include "ui_palette.h"
#include "spinball_rom.h"
#include <string>
#include <vector>
#include "imgui.h"

namespace spintool
{
	class EditorUI;
}

namespace spintool
{
	struct ColourPaletteMapping
	{
		ImColor colour;
		Uint8 palette_index;
	};
	class EditorSpriteImporter
	{
	public:
		EditorSpriteImporter(EditorUI& owning_ui);

		void Update();

		bool visible = false;

	private:
		void InnerUpdate();
		
		EditorUI& m_owning_ui;

		SDLSurfaceHandle m_imported_image;
		SDLSurfaceHandle m_preview_image;
		SDLSurfaceHandle m_export_preview_image;
		SDLTextureHandle m_rendered_imported_image;
		SDLTextureHandle m_rendered_preview_image;
		SDLTextureHandle m_export_preview_texture;

		std::vector<ColourPaletteMapping> m_detected_colours;
		int m_selected_palette_index = 0;
		VDPPalette m_selected_palette;
		SDLPaletteHandle m_preview_palette;
		std::shared_ptr<const SpinballSprite> m_result_sprite;
		std::string m_loaded_path;

		int m_target_write_location = 0;
	};
}