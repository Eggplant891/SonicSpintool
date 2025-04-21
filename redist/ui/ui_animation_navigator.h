#pragma once

#include "ui/ui_editor_window.h"
#include "rom/animation_sequence.h"

namespace spintool
{
	struct UIAnimationSequence
	{
		std::shared_ptr<const rom::AnimationSequence> anim_sequence;
		size_t current_command = 0;
	};
	class EditorAnimationNavigator : public EditorWindowBase
	{
	public:
		using EditorWindowBase::EditorWindowBase;

		void Update() override;

	private:
		void LoadPlayerAnimationTables();
		void LoadLevelAnimations(int level_index);

		std::vector<UIAnimationSequence> m_animations;
		float m_zoom = 1.0f;
	};
}