#pragma once

#include "ui/ui_editor_window.h"
#include "rom/animation_sequence.h"

#include <array>
#include <chrono>
#include <string>

namespace spintool
{
	struct UIAnimationSequence
	{
		std::shared_ptr<const rom::AnimationSequence> anim_sequence;
		Uint32 default_sprite_table = 0;     // The table the animation list was loaded with
		Uint32 loaded_sprite_table = 0;      // The table anim_sequence's sprites came from
		size_t current_visual_command = 0;
		size_t current_command = 0;          // The next command to run
		int remaining_ticks_on_frame = 0;
		static constexpr int s_default_ticks_per_frame = 6;
		int base_ticks_per_frame = s_default_ticks_per_frame;        // The game takes the initial value from the object definition
		int ticks_per_frame = 6;
		int loop_counter = 0;
		int preview_loop_count = 0;
		int preview_loop_max = 10;
		int restart_delay = 0;               // Ticks to wait before restarting a finished preview
		bool frame_hidden = false;
		bool object_flip_x = false;          // The object's own flip ($22 bit $4000); XORed with each frame's flip
		Point anim_offset{ 0,0 };
		// Animation variables: $00-$3F (the shared table, offset by the object) and $40-$45 (the object's own bytes).
		// Parent and child variables are not simulated and read as 0.
		std::array<Sint16, 0x46> variables{};

		void Tick();
		void ResetAnimation();

	private:
		[[nodiscard]] Sint32 ReadOperand(const rom::AnimationCommand& command) const;
		void WriteVariable(Uint16 index, Sint16 value);
	};

	class EditorAnimationNavigator : public EditorWindowBase
	{
	public:
		using EditorWindowBase::EditorWindowBase;

		void Update() override;

	private:
		void LoadPlayerAnimationTables();
		void LoadAnimationsScrapedFromAnimObjects();
		void LoadLevelAnimations(int level_index);

		void ApplyPalette(UIAnimationSequence& anim);
		[[nodiscard]] std::shared_ptr<const rom::Palette> ResolvePalette(Uint32 animation_offset) const;
		bool DrawMetadataEditor(UIAnimationSequence& anim);
		void FillPalettesFromLevelObjects();

		std::vector<UIAnimationSequence> m_animations;
		float m_zoom = 2.0f;
		bool m_show_animation_bounds = true;  // Red: the area the whole animation can cover
		bool m_show_frame_bounds = true;      // Yellow: the current frame
		std::string m_metadata_status;
		float m_preview_speed = 1.0f;        // 1.0 = the game's 60 ticks per second (NTSC)
		double m_tick_accumulator = 0.0;
		std::chrono::steady_clock::time_point m_last_update_time{};
	};
}
