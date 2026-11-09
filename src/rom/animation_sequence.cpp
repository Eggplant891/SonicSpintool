#include "rom/animation_sequence.h"
#include "rom/spinball_rom.h"

#include <algorithm>
#include <map>
#include <memory>
#include <set>

namespace spintool::rom
{
	const char* AnimationCommandTypeToString(const AnimationCommandType command_type)
	{
		switch (command_type)
		{
		case AnimationCommandType::KILL_OBJECT:
			return "Kill object";

		case AnimationCommandType::PAUSE:
			return "Pause";

		case AnimationCommandType::NOP:
			return "Nothing";

		case AnimationCommandType::GOTO:
			return "Go to";

		case AnimationCommandType::LOOP:
			return "Loop";

		case AnimationCommandType::SWITCH:
			return "Switch";

		case AnimationCommandType::HOLD_FRAME:
			return "Hold frame";

		case AnimationCommandType::HOLD_FRAME_RANDOM:
			return "Hold frame (random)";

		case AnimationCommandType::WAIT_FOR_FLAG:
			return "Wait for flag";

		case AnimationCommandType::SET_ANIM_TICKS_PER_FRAME:
			return "Set anim ticks per frame";

		case AnimationCommandType::CREATE_ANIM_OBJ_INSTANCE:
			return "Create Anim Obj Instance";

		case AnimationCommandType::COMPUTED_FRAME:
			return "Computed frame";

		case AnimationCommandType::CLEAR_VARIABLE:
			return "Clear variable";

		case AnimationCommandType::SET_VARIABLE:
			return "Set variable";

		case AnimationCommandType::EXECUTE_AS_FUNCTION_PTR:
			return "Execute Function Ptr";

		case AnimationCommandType::PLAY_EXCLUSIVE_AUDIO:
			return "Play Exclusive Audio";

		case AnimationCommandType::LOAD_SPRITE_TILES:
			return "Load sprite tiles";

		case AnimationCommandType::SET_HORIZONTAL_SPEED:
			return "Set Horizontal Speed";

		case AnimationCommandType::STOP_SONG:
			return "Stop song";

		case AnimationCommandType::ADD_X_OFFSET:
			return "Add XOffset";

		case AnimationCommandType::ADD_Y_OFFSET:
			return "Add YOffset";

		case AnimationCommandType::SET_X_OFFSET:
			return "Set XOffset";

		case AnimationCommandType::SET_Y_OFFSET:
			return "Set YOffset";

		case AnimationCommandType::FRAME:
			return "Frame";

		case AnimationCommandType::HIDDEN_FRAME:
			return "Hidden frame";

		case AnimationCommandType::INVALID:
			return "Invalid (locks up)";

		case AnimationCommandType::UNKNOWN:
			break;
		}

		return "Unknown";
	}

	const char* AnimationOperandModeToString(const AnimationOperandMode mode)
	{
		switch (mode)
		{
		case AnimationOperandMode::INLINE:
			return "inline";
		case AnimationOperandMode::VARIABLE:
			return "variable";
		case AnimationOperandMode::POINTER:
			return "pointer";
		case AnimationOperandMode::TABLE_LOOKUP:
			return "table lookup";
		}
		return "?";
	}

	namespace
	{
		bool IsValidCommandType(const Uint16 type)
		{
			switch (type)
			{
			case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x06: case 0x07: case 0x08:
			case 0x09: case 0x0B: case 0x0C: case 0x0D: case 0x0E: case 0x0F: case 0x10: case 0x11:
			case 0x13: case 0x14: case 0x15: case 0x20: case 0x21: case 0x25: case 0x26:
				return true;
			default:
				return false;
			}
		}

		constexpr Uint32 s_pointer_mask = 0x0FFFFFFF;
		constexpr size_t s_max_commands = 1024;
	}

	AnimationCommand AnimationSequence::DecodeCommand(const SpinballROM& src_rom, const Uint32 offset)
	{
		AnimationCommand command;
		const Uint16 word = src_rom.ReadUint16(offset);

		if ((word & 0x8000) == 0)
		{
			// Frame. AnimObj_UpdateStateMachine treats exactly $3FFF as "no sprite".
			command.command_type = word == 0x3FFF ? AnimationCommandType::HIDDEN_FRAME : AnimationCommandType::FRAME;
			command.command_data = word & 0x3FFF;
			command.flip_x = (word & 0x4000) != 0;
			command.data_size = 2;
			command.rom_data.SetROMData(offset, offset + 2);
			return command;
		}

		Uint16 type = (word >> 8) & 0x1F;
		command.operand_mode = static_cast<AnimationOperandMode>((word >> 13) & 3);
		command.argument = word & 0xFF;
		command.inline_value = static_cast<Sint8>(word & 0xFF);
		command.data_size = 1;

		// Position of the word the operand is read from; a pointer follows it.
		Uint32 operand_word_offset = offset;
		Uint32 current_offset = offset + 2;

		if (type == 0x1F)
		{
			type = word & 0xFF;
			command.argument = src_rom.ReadUint16(offset + 2);
			command.inline_value = static_cast<Sint16>(command.argument); // the game uses it as a word
			command.data_size = 2;
			operand_word_offset = offset + 2;
			current_offset = offset + 4;
		}
		command.command_data_extra_in_word = static_cast<Uint8>(command.argument);

		if (IsValidCommandType(type) == false)
		{
			command.command_type = AnimationCommandType::INVALID;
			command.command_data = command.argument;
			command.rom_data.SetROMData(offset, current_offset);
			return command;
		}
		command.command_type = static_cast<AnimationCommandType>(type);

		if (command.command_type == AnimationCommandType::COMPUTED_FRAME)
		{
			// AnimObj_UpdateAnimation returns before decoding this one; AnimObj_UpdateStateMachine reads it instead.
			if (command.operand_mode == AnimationOperandMode::POINTER)
			{
				command.extra_word = src_rom.ReadUint16(current_offset);
				command.command_data = command.extra_word;
				command.data_size = 2;
				current_offset += 2;
			}
			else
			{
				command.pointer = src_rom.ReadUint32(current_offset) & s_pointer_mask;
				command.command_data = command.pointer;
				command.data_size = 4;
				current_offset += 4;
			}
			command.rom_data.SetROMData(offset, current_offset);
			return command;
		}

		if (command.operand_mode == AnimationOperandMode::POINTER || command.operand_mode == AnimationOperandMode::TABLE_LOOKUP)
		{
			const Uint32 raw_pointer = src_rom.ReadUint32(operand_word_offset + 2);
			command.pointer = raw_pointer & s_pointer_mask;
			command.table_element_mode = static_cast<Uint8>(raw_pointer >> 28);
			command.command_data = command.pointer;
			command.data_size = 4;
			current_offset = operand_word_offset + 6;
		}
		else
		{
			command.command_data = static_cast<Uint32>(command.inline_value);
		}

		switch (command.command_type)
		{
		case AnimationCommandType::GOTO:
		case AnimationCommandType::LOOP:
			if (command.operand_mode == AnimationOperandMode::POINTER || command.operand_mode == AnimationOperandMode::TABLE_LOOKUP)
			{
				command.jump_targets.push_back(command.pointer);
			}
			break;

		case AnimationCommandType::SWITCH:
		{
			const Uint16 last_index = src_rom.ReadUint16(current_offset);
			current_offset += 2;
			for (Uint32 i = 0; i <= last_index; ++i)
			{
				command.jump_targets.push_back(src_rom.ReadUint32(current_offset) & s_pointer_mask);
				current_offset += 4;
			}
			break;
		}

		case AnimationCommandType::CREATE_ANIM_OBJ_INSTANCE:
			command.extra_x = static_cast<Sint16>(src_rom.ReadUint16(current_offset));
			command.extra_y = static_cast<Sint16>(src_rom.ReadUint16(current_offset + 2));
			current_offset += 4;
			break;

		case AnimationCommandType::SET_VARIABLE:
			if (command.operand_mode == AnimationOperandMode::INLINE)
			{
				command.extra_word = src_rom.ReadUint16(current_offset);
				current_offset += 2;
			}
			break;

		default:
			break;
		}

		command.rom_data.SetROMData(offset, current_offset);
		return command;
	}

	std::optional<size_t> AnimationSequence::FindCommandIndex(const Uint32 rom_offset) const
	{
		const auto found = std::lower_bound(std::begin(command_sequence), std::end(command_sequence), rom_offset,
			[](const AnimationCommand& command, const Uint32 offset)
			{
				return command.rom_data.rom_offset < offset;
			});

		if (found == std::end(command_sequence) || found->rom_data.rom_offset != rom_offset)
		{
			return std::nullopt;
		}
		return static_cast<size_t>(std::distance(std::begin(command_sequence), found));
	}

	std::shared_ptr<const rom::AnimationSequence> AnimationSequence::LoadFromROM(const SpinballROM& src_rom, Uint32 offset, Ptr32 sprite_table_offset)
	{
		std::shared_ptr<rom::AnimationSequence> new_animation = std::make_shared<rom::AnimationSequence>();

		// Find every command the game could reach from the start.
		std::map<Uint32, AnimationCommand> commands;
		std::vector<Uint32> to_visit{ offset };
		const Uint32 rom_size = static_cast<Uint32>(src_rom.m_buffer.size());

		while (to_visit.empty() == false && commands.size() < s_max_commands)
		{
			const Uint32 current_offset = to_visit.back();
			to_visit.pop_back();

			if (current_offset + 2 > rom_size || (current_offset & 1) != 0 || commands.find(current_offset) != std::end(commands))
			{
				continue;
			}

			AnimationCommand command = DecodeCommand(src_rom, current_offset);
			if (command.rom_data.rom_offset_end > rom_size)
			{
				continue;
			}

			bool falls_through = true;
			switch (command.command_type)
			{
			case AnimationCommandType::KILL_OBJECT:
			case AnimationCommandType::INVALID:
			case AnimationCommandType::UNKNOWN:
				falls_through = false;
				break;

			case AnimationCommandType::GOTO:
				falls_through = command.jump_targets.empty();
				break;

			case AnimationCommandType::PAUSE:
				// The game only gets past a pause if other code restarts the object's timer; in practice it switches
				// animation instead. What follows is usually another animation (e.g. runs of "frame, pause" one-frame
				// animations, each with its own pointer) or unrelated data (the flipper table), so stop here.
				falls_through = false;
				break;

			default:
				break;
			}

			for (const Uint32 target : command.jump_targets)
			{
				to_visit.push_back(target);
			}
			if (falls_through)
			{
				to_visit.push_back(command.rom_data.rom_offset_end);
			}

			commands.emplace(current_offset, std::move(command));
		}

		// Sprites for the frames.
		const Uint16 num_sprites_in_table = sprite_table_offset != 0 ? src_rom.ReadUint16(sprite_table_offset) : 0;
		for (auto& [command_offset, command] : commands)
		{
			if (command.command_type != AnimationCommandType::FRAME || command.command_data >= num_sprites_in_table)
			{
				continue;
			}

			const Ptr32 offset_table = sprite_table_offset + 4;
			const Ptr32 offset_of_sprite = sprite_table_offset + (src_rom.ReadUint16(offset_table + (command.command_data * 2)));
			if (std::shared_ptr<const rom::Sprite> new_sprite = rom::Sprite::LoadFromROM(src_rom, offset_of_sprite))
			{
				command.ui_frame_sprite = std::make_shared<UISpriteTexture>(new_sprite);
				command.ui_frame_sprite->texture = command.ui_frame_sprite->RenderTextureForPalette(UIPalette{ *src_rom.GetGlobalPalettes().at(2) }, command.flip_x);
			}
		}

		Uint32 end_offset = offset;
		new_animation->command_sequence.reserve(commands.size());
		for (auto& [command_offset, command] : commands)
		{
			end_offset = std::max(end_offset, command.rom_data.rom_offset_end);
			new_animation->command_sequence.emplace_back(std::move(command));
		}
		new_animation->start_command_index = new_animation->FindCommandIndex(offset).value_or(0);

		// Offset range: follow the default path (variables are 0, so switches fall through) for a while.
		Point current_dynamic_offset{ 0,0 };
		std::set<std::pair<size_t, int>> visited;
		std::optional<size_t> index = new_animation->start_command_index;
		int loop_counter = 0;
		for (size_t steps = 0; index.has_value() && *index < new_animation->command_sequence.size() && steps < 4096; ++steps)
		{
			if (visited.emplace(*index, loop_counter).second == false)
			{
				break;
			}
			const AnimationCommand& command = new_animation->command_sequence[*index];
			std::optional<size_t> next = new_animation->FindCommandIndex(command.rom_data.rom_offset_end);
			switch (command.command_type)
			{
			case AnimationCommandType::ADD_X_OFFSET: current_dynamic_offset.x += command.inline_value; break;
			case AnimationCommandType::ADD_Y_OFFSET: current_dynamic_offset.y += command.inline_value; break;
			case AnimationCommandType::SET_X_OFFSET:
				if (new_animation->set_x_origin.has_value() == false)
				{
					new_animation->set_x_origin = command.inline_value;
				}
				current_dynamic_offset.x = command.inline_value - *new_animation->set_x_origin;
				break;
			case AnimationCommandType::SET_Y_OFFSET:
				if (new_animation->set_y_origin.has_value() == false)
				{
					new_animation->set_y_origin = command.inline_value;
				}
				current_dynamic_offset.y = command.inline_value - *new_animation->set_y_origin;
				break;
			case AnimationCommandType::GOTO:
				next = command.jump_targets.empty() ? std::nullopt : new_animation->FindCommandIndex(command.jump_targets.front());
				break;
			case AnimationCommandType::LOOP:
				if (++loop_counter < command.argument && command.jump_targets.empty() == false)
				{
					next = new_animation->FindCommandIndex(command.jump_targets.front());
				}
				else
				{
					loop_counter = 0;
				}
				break;
			case AnimationCommandType::KILL_OBJECT:
			case AnimationCommandType::INVALID:
				next = std::nullopt;
				break;
			default:
				break;
			}

			new_animation->min_dynamic_offset.x = std::min<int>(new_animation->min_dynamic_offset.x, current_dynamic_offset.x);
			new_animation->min_dynamic_offset.y = std::min<int>(new_animation->min_dynamic_offset.y, current_dynamic_offset.y);
			new_animation->max_dynamic_offset.x = std::max<int>(new_animation->max_dynamic_offset.x, current_dynamic_offset.x);
			new_animation->max_dynamic_offset.y = std::max<int>(new_animation->max_dynamic_offset.y, current_dynamic_offset.y);
			index = next;
		}

		new_animation->rom_data.SetROMData(offset, end_offset);
		return new_animation;
	}
}
