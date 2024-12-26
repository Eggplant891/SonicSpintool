#pragma once

#include "ui/ui_editor_window.h"
#include "types/sdl_handle_defs.h"

#include <vector>
#include <memory>

namespace spintool::rom
{
	struct TileLayout;
	struct TileSet;
}

namespace spintool
{
	struct TileBrushPreview
	{
		SDLSurfaceHandle surface;
		SDLTextureHandle texture;
	};
	class EditorTileLayoutViewer : public EditorWindowBase
	{
	public:
		using EditorWindowBase::EditorWindowBase;
		void Update() override;

	private:
		SDLTextureHandle m_tile_layout_preview;
		std::vector<TileBrushPreview> m_tile_brushes_previews;
		std::shared_ptr<const rom::TileSet> m_tileset;
		std::shared_ptr<rom::TileLayout> m_tile_layout;
	};
}