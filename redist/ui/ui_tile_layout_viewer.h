#pragma once

#include "ui/ui_editor_window.h"
#include "types/sdl_handle_defs.h"

#include "rom/culling_tables/spline_culling_table.h"
#include "rom/game_objects/game_object_definition.h"
#include "rom/game_objects/game_object_flipper.h"
#include "rom/game_objects/game_object_ring.h"
#include "rom/animated_object.h"

#include "imgui.h"

#include <vector>
#include <memory>
#include <optional>
#include <string>
#include "editor/level.h"

namespace spintool
{
	class EditorUI;
	struct UISpriteTexture;
}

namespace spintool::rom
{
	struct TileLayout;
	struct TileSet;
	struct AnimationSequence;
	struct Sprite;
}

namespace spintool
{
	struct RenderTileLayoutRequest
	{
		CompressionAlgorithm compression_algorithm = CompressionAlgorithm::NONE;
		Uint32 tileset_address = 0;
		Uint32 tile_brushes_address = 0;
		Uint32 tile_brushes_address_end = 0;
		Uint32 tile_layout_address = 0;
		std::optional<Uint32> tile_layout_address_end;
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

		std::shared_ptr<const rom::TileSet>* store_tileset = nullptr;
		std::shared_ptr<rom::TileLayout>* store_layout = nullptr;
	};

	struct UIGameObject
	{
		rom::GameObjectDefinition obj_definition;
		ImVec2 pos = { 0,0 };
		ImVec2 dimensions = { 0,0 };

		int sprite_table_address = 0;
		int palette_index = 0;

		std::shared_ptr<UISpriteTexture> ui_sprite;
		SDLPaletteHandle palette;
	};

	struct TileBrushPreview
	{
		SDLSurfaceHandle surface;
		SDLTextureHandle texture;

		Uint32 brush_index;
	};

	struct WorkingGameObject
	{
		UIGameObject* destination = nullptr;
		rom::GameObjectDefinition game_obj;
	};

	struct WorkingSpline
	{
		rom::CollisionSpline* destination = nullptr;
		rom::CollisionSpline spline;
		Point* dest_spline_point = nullptr;
	};

	struct TilesetPreview
	{
		int current_page = 0;
		std::vector<TileBrushPreview> brushes;
	};

	class EditorTileLayoutViewer : public EditorWindowBase
	{
	public:
		EditorTileLayoutViewer(EditorUI& owning_ui);
		void Update() override;

		struct AnimSpriteEntry
		{
			rom::Ptr32 sprite_table = 0;
			std::shared_ptr<const rom::AnimationSequence> anim_sequence;
			size_t anim_command_used_for_surface = 0;
			SDLSurfaceHandle sprite_surface;
			SDLPaletteHandle palette;
		};

	private:
		std::shared_ptr<Level> m_level;

		SDLTextureHandle m_tile_layout_preview_bg;
		SDLTextureHandle m_tile_layout_preview_fg;
		std::vector<RenderTileLayoutRequest> m_tile_layout_render_requests;
		std::vector<TilesetPreview> m_tileset_preview_list;
		std::vector<std::unique_ptr<UIGameObject>> m_preview_game_objects;
		std::vector<AnimSpriteEntry> m_anim_sprite_instances;

		std::optional<WorkingGameObject> m_working_game_obj;
		std::optional<WorkingSpline> m_working_spline;
	};
}