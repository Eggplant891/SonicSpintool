#pragma once

#include "ui/ui_editor_window.h"
#include "ui/ui_sprite.h"
#include "ui/ui_palette_viewer.h"

#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace spintool
{
	class EditorUI;

	// A single visually unique Bonus Stage image. Multiple object instances,
	// animation states and frame entries may reference this same preview.
	// Priority is intentionally excluded from visual_tile_attributes because it
	// changes draw order, not the pixels of the image itself.
	struct BonusStageImagePreview
	{
		Uint32 mapping_offset = 0;
		Uint16 visual_tile_attributes = 0;
		std::shared_ptr<UISpriteTexture> texture;
	};

	// A complete assembled Tails plane frame. scene_mask allows genuinely shared
	// profile frames to be stored once while remaining visible in each sequence.
	struct TailsPlaneFramePreview
	{
		std::string name;
		std::string usage;
		std::size_t frame_id = 0;
		Uint8 scene_mask = 0;
		std::shared_ptr<UISpriteTexture> texture;
	};

	class EditorSpriteNavigator : public EditorWindowBase
	{
	public:
		using EditorWindowBase::EditorWindowBase;

		void Update() override;

	private:
		void LoadBonusStageImages();
		void ImportBonusStageImage(const std::filesystem::path& path, std::size_t image_index);
		void LoadTailsPlaneImages();
		void ImportTailsPlaneImage(const std::filesystem::path& path, std::size_t image_index);
		void ExportTailsPlaneImage(std::size_t image_index);
		void ImportMainSpriteImage(const std::filesystem::path& path, Uint32 sprite_rom_offset);

		std::vector<std::shared_ptr<UISpriteTexture>> m_sprites_found;
		std::vector<BonusStageImagePreview> m_bonus_stage_images;
		std::vector<TailsPlaneFramePreview> m_tails_plane_images;
		std::vector<std::shared_ptr<UISpriteTexture>> m_pending_sprites;
		std::mutex m_pending_sprites_mutex;
		std::atomic<bool> m_find_all_running{ false };
		std::atomic<float> m_find_all_progress{ 0.0f };
		std::atomic<bool> m_find_all_cancel{ false };
		std::atomic<int> m_find_all_result_count{ 0 };
		std::atomic<Uint32> m_scan_generation{ 0 };

		SDLTextureHandle m_random_texture;
		int m_arbitrary_num_tiles_width = 16;
		int m_arbitrary_num_tiles_height = 16;
		int m_arbitrary_texture_width = 128;
		int m_arbitrary_texture_height = 128;

		// m_offset remains the editable/manual offset. The applied start is kept
		// separately so browsing to the next sprite does not silently change the
		// active full-scan range.
		Uint32 m_scan_start_offset = 0x14D2;
		Uint32 m_scan_end_offset = 0x03909D;
		Uint32 m_selected_sprite_rom_offset = 0;
		Uint32 m_offset = 0x14D2;
		int m_chosen_palette = 0;
		int m_selected_bonus_stage = 0;
		int m_selected_bonus_image = -1;
		int m_selected_tails_scene = 0;
		int m_selected_tails_image = -1;
		float m_zoom = 2.0f;
		std::string m_bonus_stage_status;
		std::vector<std::string> m_bonus_stage_warnings;
		std::string m_tails_plane_status;
		std::vector<std::string> m_tails_plane_warnings;
		std::optional<std::size_t> m_bonus_import_target;
		std::optional<std::size_t> m_tails_import_target;
		std::optional<Uint32> m_main_import_target;
		std::string m_main_sprite_status;
		bool m_open_bonus_import_popup = false;
		bool m_close_bonus_import_popup = false;
		bool m_open_tails_import_popup = false;
		bool m_close_tails_import_popup = false;
		bool m_open_main_import_popup = false;
		bool m_close_main_import_popup = false;

		enum class ArbitraryRenderMode
		{
			VDP_COLOUR,
			VDP_TILES,
			PALETTE,
		};

		enum class ResultDisplayMode
		{
			MAIN_SPRITES,
			BONUS_OBJECT_FRAMES,
			TAILS_PLANE_FRAMES,
		};

		ArbitraryRenderMode m_render_arbitrary_with_palette = ArbitraryRenderMode::PALETTE;
		ResultDisplayMode m_result_display_mode = ResultDisplayMode::MAIN_SPRITES;
		bool m_attempt_render_of_arbitrary_data = false;
		bool m_show_arbitrary_render = false;
	};
}
