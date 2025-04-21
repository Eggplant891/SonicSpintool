#include "ui/ui_animation_navigator.h"

#include "ui/ui_editor.h"
#include "types/rom_ptr.h"
#include <chrono>

namespace spintool
{
	void EditorAnimationNavigator::LoadPlayerAnimationTables()
	{
		const rom::SpinballROM& rom = m_owning_ui.GetROM();

		static Uint32 anims_offset = 0x6FE;
		static Uint32 num_anims = 58;

		m_animations.reserve(num_anims);
		for (Uint32 i = 0; i < num_anims; ++i)
		{
			UIAnimationSequence sequence = { rom::AnimationSequence::LoadFromROM(rom, rom.ReadUint32(anims_offset + (i * 4)), 0x13DC) };
			m_animations.emplace_back(std::move(sequence));
		}
	}

	void EditorAnimationNavigator::LoadLevelAnimations(int level_index)
	{
		const rom::LevelDataOffsets level_offsets{ level_index };

		const rom::SpinballROM& rom = m_owning_ui.GetROM();

		const Uint32 level_anims_ptr_table_offset = level_offsets.animations;
		const Uint32 anims_offset = rom.ReadUint32(level_anims_ptr_table_offset); // 0x0003909E;
		const Uint32 sprite_table_ptr = level_offsets.sprite_table;
		const Uint32 num_anims = level_offsets.animation_count;

		m_animations.reserve(num_anims);
		for (Uint32 i = 0; i < num_anims; ++i)
		{
			UIAnimationSequence sequence = { rom::AnimationSequence::LoadFromROM(rom, rom.ReadUint32(anims_offset + (i * 4)), sprite_table_ptr) };
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
		if (ImGui::Begin("Animation Navigator", &m_visible, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar))
		{
			if (ImGui::BeginMenuBar())
			{
				if (ImGui::BeginMenu("Load"))
				{
					if (ImGui::MenuItem("Player"))
					{
						m_animations.clear();
						LoadPlayerAnimationTables();
					}

					if (ImGui::MenuItem("Global"))
					{
						m_animations.clear();
						LoadPlayerAnimationTables();
					}
					ImGui::Separator();
					if (ImGui::MenuItem("Toxic Caves"))
					{
						m_animations.clear();
						LoadLevelAnimations(0);
					}

					if (ImGui::MenuItem("Lava Powerhouse"))
					{
						m_animations.clear();
						LoadLevelAnimations(1);
					}

					if (ImGui::MenuItem("The Machine"))
					{
						m_animations.clear();
						LoadLevelAnimations(2);
					}

					if (ImGui::MenuItem("Showdown"))
					{
						m_animations.clear();
						LoadLevelAnimations(3);
					}
					ImGui::EndMenu();
				}
				ImGui::EndMenuBar();
			}

			if (m_animations.empty() == false)
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
							float smallest_offset_x = std::numeric_limits<float>::max();
							float smallest_offset_y = std::numeric_limits<float>::max();
							float biggest_offset_x = std::numeric_limits<float>::min();
							float biggest_offset_y = std::numeric_limits<float>::min();
							float largest_width = std::numeric_limits<float>::min();
							float largest_height = std::numeric_limits<float>::min();
							for (const rom::AnimationCommand& command : anim.anim_sequence->command_sequence)
							{
								const std::shared_ptr<UISpriteTexture>& frame_sprite = command.ui_frame_sprite;
								if (frame_sprite != nullptr)
								{
									smallest_offset_x = std::min<float>(smallest_offset_x, static_cast<float>(frame_sprite->sprite->GetOriginOffsetFromMinBounds().x));
									smallest_offset_y = std::min<float>(smallest_offset_y, static_cast<float>(frame_sprite->sprite->GetOriginOffsetFromMinBounds().y));
									biggest_offset_x = std::max<float>(biggest_offset_x, static_cast<float>(frame_sprite->sprite->GetOriginOffsetFromMinBounds().x));
									biggest_offset_y = std::max<float>(biggest_offset_y, static_cast<float>(frame_sprite->sprite->GetOriginOffsetFromMinBounds().y));
									largest_width = std::max<float>(largest_width, static_cast<float>(frame_sprite->dimensions.x));
									largest_height = std::max<float>(largest_height, static_cast<float>(frame_sprite->dimensions.y));
								}
							}
							const ImVec2 start_cursor_pos = ImGui::GetCursorPos();
							const ImVec2 start_cursor_screen_pos = ImGui::GetCursorScreenPos();
							ImGui::Dummy(ImVec2{
								(std::abs(biggest_offset_x - smallest_offset_x) + largest_width) * m_zoom ,
								(std::abs(biggest_offset_y - smallest_offset_y + largest_height) * m_zoom) });
							ImGui::SameLine();
							ImGui::SameLine();
							ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(255,0,0,255));
							ImGui::SetCursorPos(ImVec2{ start_cursor_pos.x + ImGui::GetItemRectMin().x - start_cursor_screen_pos.x + (biggest_offset_x * m_zoom),start_cursor_pos.y + ImGui::GetItemRectMin().y - start_cursor_screen_pos.y + (biggest_offset_y * m_zoom)});
							if (current_command.ui_frame_sprite != nullptr)
							{
								current_command.ui_frame_sprite->DrawForImGuiWithOffset(m_zoom);
								ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(255, 255, 0, 255));
							}
						}
					}
				}
			}
		}
		ImGui::End();
	}
}
