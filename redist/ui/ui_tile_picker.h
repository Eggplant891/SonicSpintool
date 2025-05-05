#pragma once

#include "types/sdl_handle_defs.h"

#include <memory>
#include <vector>

namespace spintool
{
	class EditorUI;
}

namespace spintool::rom
{
	struct SpriteTile;
	struct TileLayer;
	struct Tile;
}

namespace spintool
{
	struct TilePickerTile
	{
		std::shared_ptr<rom::SpriteTile> sprite_tile;
	};

	class TilePicker
	{
	public:
		TilePicker(EditorUI& owning_ui);
		void RenderTileset();
		void Draw();

		void SetPaletteLine(int palette_line);
		void SetTileLayer(rom::TileLayer* layer);
		rom::TileLayer* GetTileLayer();
		size_t GetSelectedTileIndex() const;
		const rom::Tile* GetSelectedTile() const;

		void DrawPickedTile(bool flip_x, bool flip_y, float zoom) const;

	//private:
		static constexpr Uint32 picker_width = 20;
		static constexpr float zoom = 2.0f;

		EditorUI& m_owning_ui;

		std::vector<std::shared_ptr<rom::SpriteTile>> tiles;
		const rom::SpriteTile* currently_selected_tile = nullptr;
		SDLSurfaceHandle surface;
		SDLTextureHandle texture;
		int current_palette_line = 0;
		Uint32 picker_height = 1;

	private:
		rom::TileLayer* m_tile_layer = nullptr;
	};
}