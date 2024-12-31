#include "ui/ui_animation_navigator.h"

#include "ui/ui_editor.h"

namespace spintool
{
	void EditorAnimationNavigator::LoadPlayerAnimationTables()
	{
		const rom::SpinballROM& rom = m_owning_ui.GetROM();

		static const size_t player_anims_offset = 0x6FE;
		static const size_t num_player_anims = 58;

		m_player_animations.animations.reserve(num_player_anims);
		for (size_t i = 0; i < num_player_anims; ++i)
		{
			rom::AnimationSequence new_sequence;
			new_sequence.rom_data.rom_offset = rom.ReadUint32(player_anims_offset + (i * 4));
			m_player_animations.animations.emplace_back(std::move(new_sequence));
		}
	}
	void EditorAnimationNavigator::Update()
	{
		if (IsOpen() == false)
		{
			return;
		}

		if (ImGui::Begin("Animation Navigator", &m_visible, ImGuiWindowFlags_NoSavedSettings))
		{
			if (ImGui::Button("Load Player Animations"))
			{
				LoadPlayerAnimationTables();
			}

			if (m_player_animations.animations.empty() == false)
			{
				if (ImGui::TreeNode("Player Animations"))
				{
					for (const rom::AnimationSequence& anim : m_player_animations.animations)
					{
						ImGui::Text("0x%08X", anim.rom_data.rom_offset);
					}

					ImGui::TreePop();
				}
			}
		}
		ImGui::End();
	}
}
