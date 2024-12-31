#pragma once

#include "ui/ui_editor_window.h"
#include "rom/animation_sequence.h"

namespace spintool
{
	class EditorAnimationNavigator : public EditorWindowBase
	{
	public:
		using EditorWindowBase::EditorWindowBase;

		void Update() override;

	private:
		void LoadPlayerAnimationTables();

		rom::AnimationTable m_player_animations;
	};
}