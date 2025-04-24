#pragma once

#include "ui/ui_editor_window.h"
#include "rom/animation_sequence.h"

namespace spintool
{
	struct UIAnimationSequence
	{
		std::shared_ptr<const rom::AnimationSequence> anim_sequence;
		size_t current_visual_command = 0;
		size_t current_command = 0;
		int remaining_ticks_on_frame = 0;
		int base_ticks_per_frame = 6;
		int loop_counter = 0;
		int preview_loop_count = 0;
		int preview_loop_max = 10;
		Point anim_offset{ 0,0 };

		void GoToNextCommand();
		void ResetAnimation();
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

		std::vector<UIAnimationSequence> m_animations;
		float m_zoom = 2.0f;
	};
}