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

		case AnimationCommandType::TERMINATE:
			return "Terminate / Skip or Pause?";

		case AnimationCommandType::DO_NOTHING:
			return "Nothing";

		case AnimationCommandType::GOTO_PREVIOUS_FRAME:
			return "Go to previous frame";

		case AnimationCommandType::RESTART_ANIMATION:
			return "Restart Animation";

		case AnimationCommandType::WAIT_FOR_N_FRAMES:
			return "Wait for N frames";

		case AnimationCommandType::EXECUTE_AS_FUNCTION_PTR:
			return "Execute Function Ptr";

		case AnimationCommandType::PLAY_EXCLUSIVE_AUDIO:
			return "Play Exclusive Audio";

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

		case AnimationCommandType::NORMAL_FRAME_FLIPPED:
			return "Normal (XFlip)";

		case AnimationCommandType::TILED_ANIM_OBJ_THING:
			return "Tiled anim obj thing";

		case AnimationCommandType::SET_REMAINING_FRAME_TIME:
			return "Set remaining frame time";

		case AnimationCommandType::SET_ANIM_TICKS_PER_FRAME:
			return "Set anim ticks per frame";

		case AnimationCommandType::UNK_INVERT_SOMETHING:
			return "Unk Invert something";

		case AnimationCommandType::END:
			return "END";

		case AnimationCommandType::FRAME_JUMP_QUESTION:
			return "Frame Jump?";

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

		while (current_offset < src_rom.m_buffer.size() - 2)
		{
			Uint8 command_code = src_rom.ReadUint8(current_offset);
			Uint16 command_data = src_rom.ReadUint8(current_offset + 1);

			if (command_code == 0x1F)
			{
				command_data = src_rom.ReadUint16(current_offset + 2);

			}

			//AnimationCommandCode command_code = DecodeCommand(command_code);
			AnimationCommand command;
			command.command_type = static_cast<AnimationCommandType>(command_code & 0x1F);
			
			if (command.command_type == AnimationCommandType::NORMAL_FRAME)
			{
				command.flip_x = command_code == 0x40;
				command_code &= ~0x40;

				const auto start_offset = current_offset;
				command.command_data = src_rom.ReadUint8(current_offset + 1);
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

			if (command.command_type == AnimationCommandType::TILED_ANIM_OBJ_THING)
			{
				const auto start_offset = current_offset;
				{
					command.command_data = src_rom.ReadUint16(current_offset);
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
					current_offset += 6;
				}
				else if ((command_code & 0x20) == 0x20)
				{
					command.command_data = (src_rom.ReadUint8(current_offset + 1) << 4) | src_rom.ReadUint16(current_offset + 2);
					current_offset += 4;
				}
				else
				{
					command.command_data = src_rom.ReadUint8(current_offset + 1);
					current_offset += 2;
				}
				command.rom_data.SetROMData(start_offset, current_offset);
				result_data.emplace_back(command);

				new_animation->rom_data.SetROMData(offset, current_offset);
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

			if (command.command_type == AnimationCommandType::GOTO_PREVIOUS_FRAME)
			{
				const auto start_offset = current_offset;
				// Jumping to another animation

				if ((command_code & 0x40) == 0x40)
				{
					command.command_data = src_rom.ReadUint32(current_offset + 2);
					current_offset += 6;
				}
				else if ((command_code & 0x20) == 0x20)
				{
					command.command_data = (src_rom.ReadUint8(current_offset + 1) << 4) | src_rom.ReadUint16(current_offset+2);
					current_offset += 4;
				}
				else
				{
					command.command_data = src_rom.ReadUint8(current_offset+1);
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
					new_animation->rom_data.SetROMData(offset, current_offset);
					break;
				}

				continue;
			}

			

			const auto start_offset = current_offset;

			if (command.command_type == AnimationCommandType::FRAME_JUMP_QUESTION)
			{
				current_offset += 4;
			}
			else
			{
				if ((command_code & 0x40) == 0x40)
				{
					command.command_data = src_rom.ReadUint32(current_offset + 2);
					current_offset += 6;
				}
				else if ((command_code & 0x20) == 0x20)
				{
					command.command_data = src_rom.ReadUint16(current_offset + 2);
					current_offset += 4;
				}
				else
				{
					command.command_data = src_rom.ReadUint8(current_offset + 1);
					current_offset += 2;
				}
			}
			command.rom_data.SetROMData(start_offset, current_offset);
			result_data.emplace_back(command);
			
		}

		new_animation->command_sequence = result_data;
		
		return new_animation;
	}
}
