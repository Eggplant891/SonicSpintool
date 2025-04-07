#pragma once

#include "ui/ui_editor_window.h"
#include <vector>
#include "SDL3/SDL_stdinc.h"
#include "rom/sprite.h"
#include "editor/level.h"

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
	class EditorTileEditor : public EditorWindowBase
	{
	public:
		EditorTileEditor(EditorUI& owning_ui, TileLayer& tile_layer, Uint32 brush_index);

		void RenderBrush();
		void Update() override;

	private:
		rom::Sprite m_brush_sprite;
		TileBrushPreview m_brush_preview;
		TileLayer* m_tile_layer = nullptr;
		rom::TileBrushBase* m_target_brush = nullptr;
		Uint32 m_brush_index = 0;
	};
}
