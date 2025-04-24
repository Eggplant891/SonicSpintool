#include "rom/animation_sequence.h"
#include "rom/spinball_rom.h"

#include <memory>

namespace spintool::rom
{
	const char* AnimationCommandTypeToString(const AnimationCommandType command_type)
	{
		switch (command_type)
		{
		case AnimationCommandType::NORMAL_FRAME:
			return "Normal";

		case AnimationCommandType::SKIP_FRAME_OR_PAUSE_ANIM:
			return "Skip or Pause anim";

		case AnimationCommandType::DO_NOTHING:
			return "Nothing";

		case AnimationCommandType::GOTO_PREVIOUS_FRAME:
			return "Go to previous frame";

		case AnimationCommandType::LOOP_COUNTER_CHECKPOINT:
			return "Check & Increment loop counter";

		case AnimationCommandType::RELATIVE_JUMP:
			return "Relative Jump";

		case AnimationCommandType::DECREMENT_REMAINING_FRAME_TIME:
			return "Decrement remaining frame time";

		case AnimationCommandType::SET_REMAINING_FRAME_TIME:
			return "Set remaining frame time";

		case AnimationCommandType::SET_ANIM_TICKS_PER_FRAME:
			return "Set anim ticks per frame";

		case AnimationCommandType::CREATE_ANIM_OBJ_INSTANCE:
			return "Create Anim Obj Instance";

		case AnimationCommandType::END:
			return "END";

		case AnimationCommandType::RESTART_ANIMATION:
			return "Restart Animation";

		case AnimationCommandType::FRAME_JUMP_QUESTION:
			return "Frame Jump?";

		case AnimationCommandType::EXECUTE_AS_FUNCTION_PTR:
			return "Execute Function Ptr";

		case AnimationCommandType::PLAY_EXCLUSIVE_AUDIO:
			return "Play Exclusive Audio";

		case AnimationCommandType::TILED_ANIM_OBJ_THING:
			return "Tiled anim obj thing";

		case AnimationCommandType::SET_HORIZONTAL_SPEED:
			return "Set Horizontal Speed";

		case AnimationCommandType::PLAY_AUDIO:
			return "Play Audio";

		case AnimationCommandType::ADD_X_OFFSET:
			return "Add XOffset";

		case AnimationCommandType::ADD_Y_OFFSET:
			return "Add YOffset";

		case AnimationCommandType::SET_X_OFFSET:
			return "Set XOffset";

		case AnimationCommandType::SET_Y_OFFSET:
			return "Set YOffset";

		case AnimationCommandType::COMMAND_MASK:
			return "Command Mask?";
		}

		return "Unknown";
	}

	AnimationCommandCode DecodeCommand(const Uint8 in_code)
	{
		// Command data structure
		// 1 bits - unknown
		// 2 bits - Data size (00 - 1, 01 - 2, 10 = 3, 11 = 4)
		// 5 bits - Data?
		// 8 bits - First byte of data (signed. -1 = invalid. Up to then 0x7F)

		constexpr Uint8 data_size_mask = 0x40 | 0x20;
		AnimationCommandCode new_command;
		
		new_command.data_size_bytes = (in_code & data_size_mask) + 1;

		new_command.flip_x = (in_code & 0x40);

		return new_command;
	}


	std::shared_ptr<const rom::AnimationSequence> AnimationSequence::LoadFromROM(const SpinballROM& src_rom, Uint32 offset, Ptr32 sprite_table_offset)
	{
		std::vector<AnimationCommand> result_data;
		Uint32 current_offset = offset;

		std::shared_ptr<rom::AnimationSequence> new_animation = std::make_shared<rom::AnimationSequence>();
		bool stop_encountered = false;

		Point current_dynamic_offset{ 0,0 };

		while (current_offset < src_rom.m_buffer.size() - 2 && stop_encountered == false)
		{
			AnimationCommand command;

			Uint8 data_read_offset = 1;

			if (src_rom.ReadUint16(current_offset) == 0x3FFF)
			{
				const auto start_offset = current_offset;
				current_offset += 2;
				command.rom_data.SetROMData(start_offset, current_offset);
				result_data.emplace_back(command);
				continue;
			}

			if (src_rom.ReadUint16(current_offset) == 0x8000)
			{
				const auto start_offset = current_offset;
				current_offset += 2;
				command.rom_data.SetROMData(start_offset, current_offset);
				result_data.emplace_back(command);
				break;
			}

			Uint8 command_code = src_rom.ReadUint8(current_offset);
			Uint16 command_data = 0;

			//if ((command_code & 0x80) == 0x80)
			//{
			//	stop_encountered = true;
			//}

			if ((command_code & 0x1F) == 0x1F)
			{
				command_code = src_rom.ReadUint8(current_offset + 1);
				command.command_type = static_cast<AnimationCommandType>(command_code);
				data_read_offset = 2;
				command_data = src_rom.ReadUint16(current_offset + data_read_offset);
			}
			else
			{
				command.command_type = static_cast<AnimationCommandType>(command_code & 0x1F);
				command_data = src_rom.ReadUint8(current_offset + data_read_offset);
			}

			//AnimationCommandCode command_code = DecodeCommand(command_code);
			
			if (command.command_type == AnimationCommandType::NORMAL_FRAME)
			{
				command.flip_x = command_code == 0x40;
				command_code &= ~0x40;

				const auto start_offset = current_offset;
				command.command_data = src_rom.ReadUint8(current_offset + data_read_offset);
				current_offset += 2;

				{
					std::shared_ptr<const rom::Sprite> current_sprite = nullptr;

					const Uint16 num_sprites_in_table = src_rom.ReadUint16(sprite_table_offset);
					const Uint16 target_sprite_index = command.command_data;
					if (target_sprite_index < num_sprites_in_table)
					{
						const Ptr32 offset_table = sprite_table_offset + 4;
						const Ptr32 offset_of_sprite = sprite_table_offset + (src_rom.ReadUint16(offset_table + (target_sprite_index * 2)));
						if (std::shared_ptr<const rom::Sprite> new_sprite = rom::Sprite::LoadFromROM(src_rom, offset_of_sprite))
						{
							current_sprite = new_sprite;
						}
					}

					if (current_sprite != nullptr)
					{
						command.ui_frame_sprite = std::make_shared<UISpriteTexture>(current_sprite);
						command.ui_frame_sprite->texture = command.ui_frame_sprite->RenderTextureForPalette(UIPalette{ *src_rom.GetGlobalPalettes().at(2) }, command.flip_x);
					}
				}

				command.rom_data.SetROMData(start_offset, current_offset);
				result_data.emplace_back(command);

				continue;
			}

			if (command.command_type == AnimationCommandType::CREATE_ANIM_OBJ_INSTANCE)
			{
				const auto start_offset = current_offset;
				command.command_data = src_rom.ReadUint32(current_offset + 2);
				current_offset += 6;
				current_offset += 2;
				current_offset += 2;
				command.rom_data.SetROMData(start_offset, current_offset);
				result_data.emplace_back(command);
				continue;
			}

			if (command.command_type == AnimationCommandType::TILED_ANIM_OBJ_THING)
			{
				const auto start_offset = current_offset;
				{
					data_read_offset = 2;

					command.command_data = src_rom.ReadUint16(current_offset + data_read_offset);
					current_offset += 2;
					command.rom_data.SetROMData(start_offset, current_offset);
					result_data.emplace_back(command);
				}
				{
					command.command_data = src_rom.ReadUint16(current_offset);
					current_offset += 2;
					command.rom_data.SetROMData(start_offset, current_offset);
					result_data.emplace_back(command);
				}
				//{
				//	AnimationCommand command;
				//	command.command_type = AnimationCommandType::UNKNOWN;
				//	command.command_data = src_rom.ReadUint16(current_offset);
				//	current_offset += 2;
				//	command.rom_data.SetROMData(start_offset, current_offset);
				//	result_data.emplace_back(command);
				//}

				continue;
			}

			if (command.command_type == AnimationCommandType::SKIP_FRAME_OR_PAUSE_ANIM)
			{
				const auto start_offset = current_offset;
				if ((command_code & 0x40) == 0x40)
				{
					command.command_data = src_rom.ReadUint32(current_offset + 2);
					command.data_size = 4;
					current_offset += 6;
				}
				else if ((command_code & 0x20) == 0x20)
				{
					command.command_data = (static_cast<Uint32>(src_rom.ReadUint8(current_offset + 1)) << 16) | src_rom.ReadUint16(current_offset + 2);
					command.data_size = 2;
					current_offset += 4;
				}
				else
				{
					command.command_data = src_rom.ReadUint8(current_offset + 1);
					command.data_size = 1;
					current_offset += 2;
				}
				command.rom_data.SetROMData(start_offset, current_offset);
				result_data.emplace_back(command);
				break;
			}

			if (command.command_type == AnimationCommandType::END)
			{
				const auto start_offset = current_offset;
				current_offset += 2;
				command.rom_data.SetROMData(start_offset, current_offset);
				result_data.emplace_back(command);
				break;
			}

			if (command.command_type == AnimationCommandType::RELATIVE_JUMP)
			{
				const auto start_offset = current_offset;
				// Jumping to another animation
				if ((command_code & 0x40) == 0x40)
				{
					command.command_data = src_rom.ReadUint32(current_offset + 2);
					command.data_size = 4;
					current_offset += 6;
				}
				else if ((command_code & 0x20) == 0x20)
				{
					command.command_data = (static_cast<Uint32>(src_rom.ReadUint8(current_offset + 1)) << 16) | src_rom.ReadUint16(current_offset + 2);
					command.data_size = 2;
					current_offset += 4;
				}
				else
				{
					command.command_data = src_rom.ReadUint8(current_offset + 1);
					command.data_size = 1;
					current_offset += 2;
				}

				command.command_data = src_rom.ReadUint32(current_offset);

				command.rom_data.SetROMData(start_offset, current_offset);
				current_offset = command.command_data;
				const bool has_looped_to_start = std::any_of(std::begin(result_data), std::end(result_data), [&command](const AnimationCommand& _command)
					{
						return _command.rom_data.rom_offset == command.command_data;
					});

				result_data.emplace_back(command);

				if (has_looped_to_start)
				{
					// we just looped to ourselves, end.
					break;
				}

				continue;
			}

			if (command.command_type == AnimationCommandType::GOTO_PREVIOUS_FRAME)
			{
				const auto start_offset = current_offset;
				// Jumping to another animation
				if ((command_code & 0x40) == 0x40)
				{
					command.command_data = src_rom.ReadUint32(current_offset + 2);
					command.data_size = 4;
					current_offset += 6;
				}
				else if ((command_code & 0x20) == 0x20)
				{
					command.command_data = (static_cast<Uint32>(src_rom.ReadUint8(current_offset + 1)) << 16) | src_rom.ReadUint16(current_offset + 2);
					command.data_size = 2;
					current_offset += 4;
				}
				else
				{
					command.command_data = src_rom.ReadUint8(current_offset + 1);
					command.data_size = 1;
					current_offset += 2;
				}
				command.rom_data.SetROMData(start_offset, current_offset);
				current_offset = command.command_data;
				const bool has_looped_to_start = std::any_of(std::begin(result_data), std::end(result_data), [&command](const AnimationCommand& _command)
					{
						return _command.rom_data.rom_offset == command.command_data;
					});

				result_data.emplace_back(command);

				if (has_looped_to_start)
				{
					// we just looped to ourselves, end.
					break;
				}

				continue;
			}

			const auto start_offset = current_offset;

			if (command.command_type == AnimationCommandType::FRAME_JUMP_QUESTION)
			{
				if ((command_code & 0x40) == 0x40)
				{
					command.command_data = src_rom.ReadUint32(current_offset + 2);
					command.data_size = 4;
					current_offset += 6;
				}
				else if ((command_code & 0x20) == 0x20)
				{
					command.command_data = (static_cast<Uint32>(src_rom.ReadUint8(current_offset + 1)) << 16) | src_rom.ReadUint16(current_offset + 2);
					command.data_size = 2;
					current_offset += 4;
				}
				else
				{
					command.command_data = src_rom.ReadUint16(current_offset + 2);
					command.data_size = 1;
					current_offset += 4;
				}
			}
			else
			{
				command.command_data_extra_in_word = src_rom.ReadUint8(current_offset + 1);
				
				if ((command_code & 0x40) == 0x40)
				{
					command.command_data = src_rom.ReadUint32(current_offset + 2);
					command.data_size = 4;
					current_offset += 6;
				}
				else if ((command_code & 0x20) == 0x20)
				{
					command.command_data = (static_cast<Uint32>(src_rom.ReadUint8(current_offset + 1)) << 16) | src_rom.ReadUint16(current_offset + 2);
					command.data_size = 2;
					current_offset += 4;
				}
				else
				{
					command.command_data = src_rom.ReadUint8(current_offset + 1);
					command.data_size = 1;
					current_offset += 2;
				}

				if (command.command_type == AnimationCommandType::ADD_X_OFFSET)
				{
					Uint16 offset_command_value = static_cast<Uint16>(command.command_data);
					if (offset_command_value > std::numeric_limits<Sint16>::max())
					{
						current_dynamic_offset.x -= (static_cast<int>(std::numeric_limits<Uint16>::max()) - offset_command_value) + 1;
					}
					else
					{
						current_dynamic_offset.x += offset_command_value;
					}
				}

				if (command.command_type == AnimationCommandType::ADD_Y_OFFSET)
				{
					Uint16 offset_command_value = static_cast<Uint16>(command.command_data);
					if (offset_command_value > std::numeric_limits<Sint16>::max())
					{
						current_dynamic_offset.y -= (static_cast<int>(std::numeric_limits<Uint16>::max()) - offset_command_value) + 1;
					}
					else
					{
						current_dynamic_offset.y += offset_command_value;
					}
				}

				new_animation->min_dynamic_offset.x = std::min<int>(new_animation->min_dynamic_offset.x, current_dynamic_offset.x);
				new_animation->min_dynamic_offset.y = std::min<int>(new_animation->min_dynamic_offset.y, current_dynamic_offset.y);
				new_animation->max_dynamic_offset.x = std::max<int>(new_animation->max_dynamic_offset.x, current_dynamic_offset.x);
				new_animation->max_dynamic_offset.y = std::max<int>(new_animation->max_dynamic_offset.y, current_dynamic_offset.y);
			}
			command.rom_data.SetROMData(start_offset, current_offset);
			result_data.emplace_back(command);
		}

		new_animation->rom_data.SetROMData(offset, current_offset);
		new_animation->command_sequence = result_data;
		
		return new_animation;
	}
}
