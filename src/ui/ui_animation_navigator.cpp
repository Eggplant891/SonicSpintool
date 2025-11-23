#include "ui/ui_animation_navigator.h"

#include "ui/ui_editor.h"
#include "types/rom_ptr.h"
#include <chrono>

namespace spintool
{
	void UIAnimationSequence::GoToNextCommand()
	{
		if (current_command + 1 >= anim_sequence->command_sequence.size())
		{
			anim_offset = { 0,0 };
			loop_counter = 0;
		}
		current_command = (current_command + 1) % anim_sequence->command_sequence.size();
	}

	void UIAnimationSequence::ResetAnimation()
	{
		anim_offset = { 0,0 };
		preview_loop_count = 0;
		remaining_ticks_on_frame = base_ticks_per_frame;
		current_command = 0;
		current_visual_command = 0;
		loop_counter = 0;
	}

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

	void EditorAnimationNavigator::LoadAnimationsScrapedFromAnimObjects()
	{
		static Uint32 anim_objs_start = 0x000C0B14;
		static Uint32 anim_objs_end = 0x000C13F0;
		static Uint32 num_anims = (anim_objs_end - anim_objs_start) / rom::AnimObjectDefinition::s_size_on_rom;

		m_animations.reserve(num_anims);

		for (Uint32 current_anim_obj = anim_objs_start; current_anim_obj != anim_objs_end; current_anim_obj += rom::AnimObjectDefinition::s_size_on_rom)
		{
			const rom::AnimObjectDefinition anim_obj = rom::AnimObjectDefinition::LoadFromROM(m_owning_ui.GetROM(), current_anim_obj);
			if (anim_obj.starting_animation != 0)
			{
				UIAnimationSequence sequence = { rom::AnimationSequence::LoadFromROM(m_owning_ui.GetROM(), anim_obj.starting_animation, m_owning_ui.GetROM().ReadUint32(anim_obj.sprite_table)) };
				m_animations.emplace_back(std::move(sequence));
			}
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
		static const auto milliseconds_per_tick = std::chrono::milliseconds(17);
		const bool tick_this_frame = duration_since_last_update >= milliseconds_per_tick;

		if (tick_this_frame)
		{
			since_last_frame_tick = now;
		}

		ImGui::SetNextWindowSize(ImVec2(800, 800), ImGuiCond_Appearing);
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
					ImGui::Separator();
					if (ImGui::MenuItem("Scrape AnimObjects"))
					{
						m_animations.clear();
						LoadAnimationsScrapedFromAnimObjects();
					}
					ImGui::EndMenu();
				}
				ImGui::EndMenuBar();
			}

			ImGui::SliderFloat("Zoom", &m_zoom, 0.0f, 8.0f, "%.1f");

			if (ImGui::BeginChild("animations"))
			{
				if (m_animations.empty() == false)
				{
					for (UIAnimationSequence& anim : m_animations)
					{
						const char* small_command = "0x%02X";
						const char* medium_command = "0x%04X";
						const char* large_command = "0x%08X";

						char id_buffer[64];
						sprintf(id_buffer, "0x%08X", static_cast<rom::Ptr32>(anim.anim_sequence->rom_data.rom_offset));
						if (ImGui::TreeNode(id_buffer))
						{
							char inner_buffer[256];

							for (const rom::AnimationCommand& command : anim.anim_sequence->command_sequence)
							{
								const char* command_data_format = command.data_size == 1 ? small_command : (command.data_size == 2 ? medium_command : large_command);
								char format_buffer[32];
								sprintf(format_buffer, "%s <%%s (0x%%02X)>###%%08X", command_data_format);
								sprintf(inner_buffer, format_buffer, command.command_data, rom::AnimationCommandTypeToString(command.command_type), static_cast<Uint8>(command.command_type), command.rom_data.rom_offset);
								
								if(ImGui::TreeNode(inner_buffer))
								{
									Uint32 current_offset = command.rom_data.rom_offset;
									Uint32 end_offset = command.rom_data.rom_offset_end;

									for (; current_offset != end_offset; current_offset += 2)
									{
										ImGui::Text("[0x%08X]:", current_offset);
										ImGui::SameLine();
										ImGui::Text("0x%04X", m_owning_ui.GetROM().ReadUint16(current_offset));
									}
									ImGui::TreePop();
								}
								if (command.ui_frame_sprite != nullptr)
								{
									command.ui_frame_sprite->DrawForImGui(2.0f);
								}
							}
							ImGui::TreePop();
						}
						else
						{
							if (anim.remaining_ticks_on_frame == 0)
							{
								anim.remaining_ticks_on_frame = anim.base_ticks_per_frame - 1;
							}

							if (anim.remaining_ticks_on_frame > 0)
							{
								--anim.remaining_ticks_on_frame;
							}

							if (tick_this_frame && anim.remaining_ticks_on_frame <= 0)
							{
								anim.GoToNextCommand();

								while (true)
								{
									const auto current_command_index = anim.current_command;

									const std::vector<rom::AnimationCommand>& current_anim_sequence = anim.anim_sequence->command_sequence;
									const rom::AnimationCommand& current_anim_command = current_anim_sequence.at(current_command_index);

									if (current_anim_command.command_type == rom::AnimationCommandType::ADD_X_OFFSET)
									{
										Uint16 offset_command_value = static_cast<Uint16>(current_anim_command.command_data);
										if (offset_command_value > std::numeric_limits<Sint16>::max())
										{
											anim.anim_offset.x -= (static_cast<int>(std::numeric_limits<Uint16>::max()) - offset_command_value) + 1;
										}
										else
										{
											anim.anim_offset.x += offset_command_value;
										}
									}
									else if(current_anim_command.command_type == rom::AnimationCommandType::ADD_Y_OFFSET)
									{
										Uint16 offset_command_value = static_cast<Uint16>(current_anim_command.command_data);
										if (offset_command_value > std::numeric_limits<Sint16>::max())
										{
											anim.anim_offset.y -= (static_cast<int>(std::numeric_limits<Uint16>::max()) - offset_command_value) + 1;
										}
										else
										{
											anim.anim_offset.y += offset_command_value;
										}
									}
									else  if (current_anim_command.command_type == rom::AnimationCommandType::LOOP_COUNTER_CHECKPOINT)
									{
										if (anim.loop_counter < current_anim_command.command_data_extra_in_word)
										{
											++anim.loop_counter;

											const Uint32 target_frame_offset = current_anim_command.command_data;
											auto target_frame = std::find_if(std::begin(current_anim_sequence), std::end(current_anim_sequence),
												[target_frame_offset](const rom::AnimationCommand& command)
												{
													return command.rom_data.rom_offset == target_frame_offset;
												});

											if (target_frame != std::end(anim.anim_sequence->command_sequence))
											{
												anim.current_command = std::distance(std::begin(current_anim_sequence), target_frame);
												continue;
											}
										}
									}
									else if (current_anim_command.command_type == rom::AnimationCommandType::GOTO_PREVIOUS_FRAME)
									{
										const Uint32 target_frame_offset = current_anim_command.command_data;
										auto target_frame = std::find_if(std::begin(current_anim_sequence), std::end(current_anim_sequence),
											[target_frame_offset](const rom::AnimationCommand& command)
											{
												return command.rom_data.rom_offset == target_frame_offset;
											});

										if (target_frame != std::end(anim.anim_sequence->command_sequence))
										{
											anim.current_command = std::distance(std::begin(current_anim_sequence), target_frame);
											anim.preview_loop_count++;
											if (anim.preview_loop_count >= anim.preview_loop_max)
											{
												anim.ResetAnimation();
											}
											continue;
										}
									}
									else if (current_anim_command.command_type == rom::AnimationCommandType::NORMAL_FRAME)
									{
										anim.current_command = current_command_index;
										anim.current_visual_command = current_command_index;
										break;
									}
									//else if (anim.anim_sequence->command_sequence.at(current_command_index).command_type == rom::AnimationCommandType::SET_HORIZONTAL_SPEED)
									//{
									//	anim.base_ticks_per_frame = anim.anim_sequence->command_sequence.at(current_command_index).command_data;
									//	anim.remaining_ticks_on_frame = anim.base_ticks_per_frame - 1;
									//	continue;
									//}
									else if (anim.anim_sequence->command_sequence.at(current_command_index).command_type == rom::AnimationCommandType::DECREMENT_REMAINING_FRAME_TIME)
									{
										anim.current_command = current_command_index;
										anim.remaining_ticks_on_frame = anim.anim_sequence->command_sequence.at(current_command_index).command_data;
										break;
									}
									else if (current_command_index == anim.current_command)
									{
										break;
									}

									anim.GoToNextCommand();
								}
							}

							const std::vector<rom::AnimationCommand>& current_anim_sequence = anim.anim_sequence->command_sequence;
							const rom::AnimationCommand& current_anim_command = current_anim_sequence.at(anim.current_command);
							const rom::AnimationCommand& current_visual_command = current_anim_sequence.at(anim.current_visual_command);

							auto& current_command = current_visual_command;
							ImVec2 smallest_offset{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
							ImVec2 biggest_offset{ std::numeric_limits<float>::min(), std::numeric_limits<float>::min() };
							ImVec2 largest_dimensions{ std::numeric_limits<float>::min(), std::numeric_limits<float>::min() };

							for (const rom::AnimationCommand& command : current_anim_sequence)
							{
								const std::shared_ptr<UISpriteTexture>& frame_sprite = command.ui_frame_sprite;
								if (frame_sprite != nullptr)
								{
									smallest_offset.x = std::min<float>(smallest_offset.x, static_cast<float>(frame_sprite->sprite->GetOriginOffsetFromMinBounds().x));
									smallest_offset.y = std::min<float>(smallest_offset.y, static_cast<float>(frame_sprite->sprite->GetOriginOffsetFromMinBounds().y));
									biggest_offset.x = std::max<float>(biggest_offset.x, static_cast<float>(frame_sprite->sprite->GetOriginOffsetFromMinBounds().x));
									biggest_offset.y = std::max<float>(biggest_offset.y, static_cast<float>(frame_sprite->sprite->GetOriginOffsetFromMinBounds().y));
									largest_dimensions.x = std::max<float>(largest_dimensions.x, static_cast<float>(frame_sprite->dimensions.x));
									largest_dimensions.y = std::max<float>(largest_dimensions.y, static_cast<float>(frame_sprite->dimensions.y));
								}
							}
							const ImVec2 start_cursor_pos = ImGui::GetCursorPos();
							const ImVec2 start_cursor_screen_pos = ImGui::GetCursorScreenPos();
							ImGui::Dummy((((biggest_offset + anim.anim_sequence->max_dynamic_offset) - (smallest_offset + anim.anim_sequence->min_dynamic_offset)) + (largest_dimensions)) * m_zoom);
							ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(255, 0, 0, 255));
							if (current_command.ui_frame_sprite != nullptr)
							{
								ImGui::SameLine();
								ImGui::SameLine();
								ImGui::SetCursorPos(start_cursor_pos + ImGui::GetItemRectMin() - start_cursor_screen_pos + ((biggest_offset + anim.anim_offset - anim.anim_sequence->min_dynamic_offset) * m_zoom));
								current_command.ui_frame_sprite->DrawForImGuiWithOffset(m_zoom);
								ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(255, 255, 0, 255));
							}
						}
					}
				}
			}
			ImGui::EndChild();
		}
		ImGui::End();
	}
}
