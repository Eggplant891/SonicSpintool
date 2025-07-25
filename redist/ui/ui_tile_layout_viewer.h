#pragma once

#include "types/sdl_handle_defs.h"

#include "rom/tile.h"
#include "rom/tile_brush.h"
#include "rom/level.h"
#include "rom/culling_tables/spline_culling_table.h"
#include "rom/game_objects/game_object_flipper.h"
#include "rom/game_objects/game_object_ring.h"
#include "rom/palette.h"

#include "editor/spline_manager.h"
#include "editor/game_obj_manager.h"

#include "ui/ui_editor_window.h"
#include "ui/ui_tile_editor.h"
#include "ui/ui_tile_picker.h"

#include "imgui.h"

#include <vector>
#include <memory>
#include <optional>
#include <string>
#include "render.h"

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
	struct TileLayer;
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

	struct WorkingGameObject
	{
		std::optional<ImVec2> initial_drag_offset;
		UIGameObject* destination = nullptr;
		UIGameObject game_obj;
	};

	struct WorkingFlipperObject
	{
		std::optional<ImVec2> initial_drag_offset;
		rom::FlipperInstance* destination = nullptr;
		rom::FlipperInstance flipper_obj;
	};

	struct WorkingRingObject
	{
		std::optional<ImVec2> initial_drag_offset;
		rom::RingInstance* destination = nullptr;
		rom::RingInstance ring_obj;
	};

	enum class WorkingSplineMode
	{
		NONE,
		PLACING_START_POINT,
		PLACING_END_POINT,
		DRAGGING_POINT,
		DRAGGING_WHOLE_SPLINE,
		EDITING_SPLINE_PROPERTIES
	};

	struct WorkingSpline
	{
		rom::CollisionSpline* destination = nullptr;
		rom::CollisionSpline spline;
		Point* dest_spline_point = nullptr;

		WorkingSplineMode current_mode = WorkingSplineMode::NONE;
	};

	struct TilesetPreview
	{
		int current_page = 0;
		std::vector<TileBrushPreview> brushes;
	};

	struct SpriteObjectPreview
	{
		SDLSurfaceHandle sprite;
		SDLPaletteHandle palette;
		SDLTextureHandle texture;
	};

	struct LayerSettings
	{
		bool background = true;
		bool foreground = true;
		bool foreground_high_priority = true;
		bool rings = true;
		bool flippers = true;
		bool invisible_objects = true;
		bool visible_objects = true;
		bool collision = true;
		bool spline_culling = false;
		bool collision_culling = false;
		bool visibility_culling = false;

		bool hover_game_objects = true;
		bool hover_game_objects_tooltip = true;
		bool hover_splines = true;
		bool hover_radials = true;
		bool hover_brushes = false;
	};

	struct TileSelection
	{
		rom::TileLayer* tile_layer = nullptr;
		const rom::Tile* tile_selection = nullptr;
		TilePicker* tile_picker = nullptr;
		bool is_picking_from_layout = false;
		bool was_picked_from_layout = false;
		std::optional<ImVec2> dragging_start_ref;
		bool flip_x = false;
		bool flip_y = false;

		void Clear()
		{
			*this = TileSelection{};
		}

		bool IsPickingFromLayout() const
		{
			return is_picking_from_layout;
		}

		bool HasSelection() const
		{
			return (tile_layer != nullptr && tile_selection != nullptr);
		}

		bool IsActive() const
		{
			return (tile_layer != nullptr && tile_selection != nullptr) || (is_picking_from_layout);
		}
	};

	struct TileBrushSelection
	{
		SDLSurfaceHandle brush_surface;
		SDLTextureHandle brush_texture;

		rom::TileLayer* tile_layer = nullptr;
		TilePicker* tile_picker = nullptr;
		std::optional<rom::TileBrush> brush;

		Uint32 BrushWidth() const { return brush ? brush->BrushWidth() : 1; };
		Uint32 BrushHeight() const { return brush ? brush->BrushHeight() : 1; }

		std::optional<ImVec2> dragging_start_ref;
		bool is_picking_from_layout = false;
		bool was_picked_from_layout = false;
		bool flip_x = false;
		bool flip_y = false;

		void Clear()
		{
			tile_layer = nullptr;
			tile_picker = nullptr;
			brush.reset();
			dragging_start_ref.reset();
			is_picking_from_layout = false;
			was_picked_from_layout = false;
			flip_x = false;
			flip_y = false;
		}

		void StartPickingFromLayout(rom::TileLayer& layer, TilePicker& picker)
		{
			brush.reset();
			dragging_start_ref.reset();
			is_picking_from_layout = true;
			was_picked_from_layout = false;
			flip_x = false;
			flip_y = false;

			tile_layer = &layer;
			tile_picker = &picker;
		}

		void PickBrush(const rom::TileBrush& new_brush)
		{
			brush = new_brush;
			brush_surface = brush->RenderToSurface(*tile_layer);
			brush_texture = Renderer::RenderToTexture(brush_surface.get());

			if (is_picking_from_layout)
			{
				is_picking_from_layout = false;
				was_picked_from_layout = true;
			}
		}

		bool IsPickingFromLayout() const
		{
			return is_picking_from_layout;
		}

		bool HasSelection() const
		{
			return (tile_layer != nullptr && brush.has_value());
		}

		bool IsActive() const
		{
			return (tile_layer != nullptr && brush.has_value()) || (is_picking_from_layout);
		}
	};

	enum RenderRequestType
	{
		NONE,
		LEVEL,
		MENU,
		OPTIONS,
		BONUS,
		SEGA_LOGO,
		INTRO,
		INTRO_SHIP,
		INTRO_WATER,
		COUNT
	};

	struct RenderRequestSettings
	{
		RenderRequestType request_type;
		bool show_background = true;
		bool show_foreground = true;
		bool show_game_objects = true;
	};

	struct PopupMessage
	{
		std::string title;
		std::string body;
	};

	class EditorTileLayoutViewer : public EditorWindowBase
	{
	public:
		EditorTileLayoutViewer(EditorUI& owning_ui);
		void Update() override;
		void DrawSidebar(bool& has_just_selected_item);
		void DrawViewport(bool& has_just_selected_item);
		void DrawLevelInfo();
		void DrawObjectTable();
		void DrawRingsTable();
		RenderRequestType DrawMenuBar();
		void DrawFlippersTable();
		void PrepareRenderRequest(RenderRequestType render_request);

		bool IsDraggingObject() const;
		bool IsObjectPopupOpen() const;
		bool IsEditingSomething() const;

		struct AnimSpriteEntry
		{
			rom::Ptr32 sprite_table = 0;
			std::shared_ptr<const rom::AnimationSequence> anim_sequence;
			size_t anim_command_used_for_surface = 0;
			SDLSurfaceHandle sprite_surface;
			SDLPaletteHandle palette;
		};

	private:
		void TestCollisionCullingResults() const;
		void Reset();

		void DrawCollisionSpline(rom::CollisionSpline& spline, ImVec2 origin, ImVec2 screen_origin, LayerSettings& current_layer_settings, bool is_working_spline, bool draw_bbox = false);
		std::shared_ptr<rom::Level> m_level;

		LayerSettings m_layer_settings;
		SplineManager m_spline_manager;
		GameObjectManager m_game_object_manager;

		std::vector<RenderTileLayoutRequest> m_tile_layout_render_requests;
		std::vector<TilePicker> m_tile_picker_list;
		std::vector<TilesetPreview> m_tileset_preview_list;
		std::vector<AnimSpriteEntry> m_anim_sprite_instances;

		std::shared_ptr<const rom::TileSet> m_working_tileset;
		std::shared_ptr<rom::TileLayout> m_working_tile_layout;
		rom::PaletteSet m_working_palette_set;
		std::optional<WorkingGameObject> m_working_game_obj;
		std::optional<WorkingFlipperObject> m_working_flipper;
		std::optional<WorkingRingObject> m_working_ring;
		std::optional<WorkingSpline> m_working_spline;

		TileBrushSelection m_selected_brush;
		TileSelection m_selected_tile;
		std::optional<Uint32> m_working_layer_index;
		std::optional<Uint32> m_working_brush;

		std::optional<EditorTileEditor> m_brush_editor;
		std::optional<RenderTileLayoutRequest> m_current_preview_data;

		rom::SplineCullingTable m_working_culling_table;

		SDLTextureHandle m_tile_layout_preview_bg;
		SDLTextureHandle m_tile_layout_preview_fg;
		SpriteObjectPreview m_flipper_preview;
		SpriteObjectPreview m_ring_preview;
		SpriteObjectPreview m_game_object_preview;

		std::optional<PopupMessage> m_popup_msg;

		float m_zoom = 1.0f;
		int m_grid_snap = 8;
		Uint8 m_sidebar_hover = 0;

		bool m_export_result = false;
		bool m_preview_bonus_alt_palette = false;
		bool m_render_from_edit = false;
		bool m_request_open_obj_popup = false;

	};
}