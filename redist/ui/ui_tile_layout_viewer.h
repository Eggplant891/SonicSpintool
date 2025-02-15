#pragma once

#include "ui/ui_editor_window.h"
#include "types/sdl_handle_defs.h"

#include <vector>
#include <memory>
#include <optional>
#include <string>

namespace spintool::rom
{
	struct TileLayout;
	struct TileSet;
}

namespace spintool
{
	enum class CompressionAlgorithm
	{
		NONE,
		SSC,
		BOOGALOO
	};
	struct RenderTileLayoutRequest
	{
		CompressionAlgorithm compression_algorithm = CompressionAlgorithm::NONE;
		size_t tileset_address = 0;
		size_t tile_brushes_address = 0;
		size_t tile_brushes_address_end = 0;
		size_t tile_layout_address = 0;
		std::optional<size_t> tile_layout_address_end;
		std::optional<Uint16> palette_line;
		unsigned int tile_layout_width = 0;
		unsigned int tile_layout_height = 0;
		int tile_brush_width = 0;
		int tile_brush_height = 0;
		std::string layout_type_name;
		std::string layout_layout_name;
		bool is_chroma_keyed = false;
		bool show_brush_previews = true;
		bool draw_mirrored_layout = false;
	};
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