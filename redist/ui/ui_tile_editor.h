#pragma once

#include "ui/ui_editor_window.h"
#include "rom/level.h"
#include "rom/sprite.h"
#include "rom/tileset.h"
#include "rom/tile_brush.h"
#include "ui_tile_picker.h"

#include "SDL3/SDL_stdinc.h"

namespace spintool
{
	struct TileLayer;
}

namespace spintool
{
	class EditorTileEditor : public EditorWindowBase
	{
	public:
		EditorTileEditor(EditorUI& owning_ui, rom::TileLayer& tile_layer, Uint32 brush_index);
		EditorTileEditor(EditorUI& owning_ui, rom::TileLayer& tile_layer, rom::TileBrush& brush_instance);

		void RenderBrush();
		void Update() override;

	private:
		rom::Sprite m_brush_sprite;
		TileBrushPreview m_brush_preview;
		
		rom::TileLayer* m_tile_layer = nullptr;
		rom::TileBrush* m_target_brush = nullptr;

		TilePicker m_tile_picker;
		Uint32 m_brush_index = 0;

		bool m_tile_brush_changed = false;
	};
}
