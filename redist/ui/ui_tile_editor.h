#pragma once

#include "ui/ui_editor_window.h"
#include "rom/level.h"
#include "rom/sprite.h"
#include "rom/sprite_tile.h"
#include "types/sdl_handle_defs.h"

#include "SDL3/SDL_stdinc.h"
#include <vector>

namespace spintool
{
	struct TileLayer;
	namespace rom
	{
		class TileBrushBase;
	}
}

namespace spintool
{
	struct TilePickerTile
	{
		std::shared_ptr<rom::SpriteTile> sprite_tile;
	};

	struct TilePicker
	{
		std::vector<std::shared_ptr<rom::SpriteTile>> tiles;
		const rom::SpriteTile* currently_selected_tile = nullptr;
		SDLSurfaceHandle surface;
		SDLTextureHandle texture;
		int current_palette_line = 0;
		static constexpr Uint32 picker_width = 16;
		Uint32 picker_height = 1;
		static constexpr float zoom = 4.0f;
	};

	class EditorTileEditor : public EditorWindowBase
	{
	public:
		EditorTileEditor(EditorUI& owning_ui, TileLayer& tile_layer, Uint32 brush_index);

		void RenderBrush();
		void RenderTileset();
		void DrawTilePicker();
		void Update() override;

	private:
		rom::Sprite m_brush_sprite;
		TileBrushPreview m_brush_preview;
		
		TileLayer* m_tile_layer = nullptr;
		rom::TileBrushBase* m_target_brush = nullptr;

		TilePicker m_tile_picker;
		Uint32 m_brush_index = 0;

		bool m_tile_brush_changed = false;
	};
}
