#include "ui/ui_animation_navigator.h"

#include "ui/ui_editor.h"
#include "types/rom_ptr.h"
#include <chrono>

namespace spintool
{
	void EditorAnimationNavigator::LoadPlayerAnimationTables()
	{
		const rom::SpinballROM& rom = m_owning_ui.GetROM();

		static size_t player_anims_offset = 0x6FE;
		static size_t num_player_anims = 58;

		m_animations.reserve(num_player_anims);
		for (size_t i = 0; i < num_player_anims; ++i)
		{
			UIAnimationSequence sequence = { rom::AnimationSequence::LoadFromROM(rom, rom.ReadUint32(player_anims_offset + (i * 4)), 0x13DC) };
			m_animations.emplace_back(std::move(sequence));
		}
	}

	void EditorAnimationNavigator::LoadToxicCavesAnimationTables()
	{
		const rom::SpinballROM& rom = m_owning_ui.GetROM();

		static size_t player_anims_offset = 0x0003909e;
		static size_t num_player_anims = 0x3E;

		m_animations.reserve(num_player_anims);
		for (size_t i = 0; i < num_player_anims; ++i)
		{
			UIAnimationSequence sequence = { rom::AnimationSequence::LoadFromROM(rom, rom.ReadUint32(player_anims_offset + (i * 4)), 0x12B0C) };
			m_animations.emplace_back(std::move(sequence));
		}
	}

	void EditorAnimationNavigator::Update()
	{
		if (IsOpen() == false)
		{
			return;
		}

		static std::chrono::steady_clock clock;
		static std::chrono::steady_clock::time_point since_last_frame_tick = clock.now();
		std::chrono::steady_clock::time_point now = clock.now();
		auto duration_since_last_update = now - since_last_frame_tick;
		static const auto milliseconds_per_tick = std::chrono::milliseconds(100);
		const bool tick_this_frame = duration_since_last_update >= milliseconds_per_tick;

		if (tick_this_frame)
		{
			since_last_frame_tick = now;
		}

		ImGui::SetNextWindowSize(ImVec2(256, 768), ImGuiCond_Appearing);
		if (ImGui::Begin("Animation Navigator", &m_visible, ImGuiWindowFlags_NoSavedSettings))
		{
			if (ImGui::Button("Load Player Animations"))
			{
				m_animations.clear();
				LoadPlayerAnimationTables();
			}

			if (ImGui::Button("Load Toxic Caves Animations"))
			{
				m_animations.clear();
				LoadToxicCavesAnimationTables();
			}

			if (m_animations.empty() == false)
			{
				if (ImGui::TreeNode("Animations"))
				{
					for (UIAnimationSequence& anim : m_animations)
					{
						char id_buffer[64];
						sprintf_s(id_buffer, "0x%08X", static_cast<rom::Ptr32>(anim.anim_sequence->rom_data.rom_offset));
						if (ImGui::TreeNode(id_buffer))
						{
							for (const rom::AnimationCommand& command : anim.anim_sequence->command_sequence)
							{
								ImGui::Text("0x%08X", command.command_data);
								ImGui::SameLine();
								ImGui::Text("<%s>", rom::AnimationCommandTypeToString(command.command_type));
								if (command.ui_frame_sprite != nullptr)
								{
									command.ui_frame_sprite->DrawForImGui(2.0f);
								}
							}
							ImGui::TreePop();
						}
						else
						{
							if (tick_this_frame)
							{
								auto current_command_index = anim.current_command;
								while (true)
								{
									current_command_index = (current_command_index + 1) % anim.anim_sequence->command_sequence.size();
									if (current_command_index == anim.current_command)
									{
										break;
									}

									if (anim.anim_sequence->command_sequence.at(current_command_index).command_type == rom::AnimationCommandType::NORMAL_FRAME)
									{
										anim.current_command = current_command_index;
										break;
									}
								}
							}

							if (anim.anim_sequence->command_sequence.at(anim.current_command).command_type == rom::AnimationCommandType::NORMAL_FRAME)
							{
								float m_zoom = 2.0f;
								auto& current_command = anim.anim_sequence->command_sequence.at(anim.current_command);
								float biggest_offset_x = std::numeric_limits<float>::min();
								float biggest_offset_y = std::numeric_limits<float>::min();
								float largest_width = std::numeric_limits<float>::min();
								float largest_height = std::numeric_limits<float>::min();
								for (const rom::AnimationCommand& command : anim.anim_sequence->command_sequence)
								{
									if (command.ui_frame_sprite != nullptr)
									{
										biggest_offset_x = std::max<float>(biggest_offset_x, static_cast<float>(command.ui_frame_sprite->sprite->GetOriginOffsetFromMinBounds().x));
										biggest_offset_y = std::max<float>(biggest_offset_y, static_cast<float>(command.ui_frame_sprite->sprite->GetOriginOffsetFromMinBounds().y));
										largest_width = std::max<float>(largest_width, std::max<float>(static_cast<float>(command.ui_frame_sprite->dimensions.y), static_cast<float>(command.ui_frame_sprite->sprite->GetOriginOffsetFromMinBounds().x)));
										largest_height = std::max<float>(largest_height, static_cast<float>(command.ui_frame_sprite->dimensions.y));
									}
								}

								ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (biggest_offset_y * m_zoom));
								ImGui::Dummy(ImVec2(8 + biggest_offset_x * m_zoom, (largest_height * m_zoom) - ImGui::GetStyle().ItemSpacing.y * 2));
								ImGui::SameLine();
								ImGui::SameLine();
								current_command.ui_frame_sprite->DrawForImGuiWithOffset(m_zoom);
							}
						}
					}

					ImGui::TreePop();
				}
			}
		}
		ImGui::End();
	}
}
