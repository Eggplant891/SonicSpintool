#pragma once

#include "rom_data.h"

#include <vector>
#include <memory>
#include "ui/ui_sprite.h"
#include "types/rom_ptr.h"

namespace spintool::rom
{
	class SpinballROM;
	struct Sprite;

	enum class AnimationCommandType
	{
		NORMAL_FRAME = 0x00,
		NORMAL_FRAME_FLIPPED = 0x40,
		TERMINATE = 0x01,
		SKIP_FRAME_OR_PAUSE_ANIM = 0x01,
		DO_NOTHING = 0x02,
		GOTO_PREVIOUS_FRAME = 0x03,
		RESTART_ANIMATION = 0x0E,
		JUMP_TO_LOCATION = 0x0F,
		EXECUTE_AS_FUNCTION_PTR = 0x10,
		PLAY_EXCLUSIVE_AUDIO = 0x11,
		SET_HORIZONTAL_SPEED = 0x14,
		PLAY_AUDIO = 0x15,
		ADD_X_OFFSET = 0x20,
		ADD_Y_OFFSET = 0x21,
		SET_X_OFFSET = 0x25,
		SET_Y_OFFSET = 0x26,

		UNKNOWN = -1,
	};
	const char* AnimationCommandTypeToString(const AnimationCommandType command_type);

	struct AnimationCommandCode
	{
		int data_size_bytes = 0;
		bool flip_x = false;
		AnimationCommandType type = AnimationCommandType::UNKNOWN;
	};

	struct AnimationCommand
	{
		ROMData rom_data;
		AnimationCommandType command_type = AnimationCommandType::NORMAL_FRAME;
		std::shared_ptr<UISpriteTexture> ui_frame_sprite;
		Uint32 command_data = 0;

		bool flip_x = false;
		bool flip_y = false;
	};

	struct AnimationSequence
	{
		ROMData rom_data;
		std::vector<AnimationCommand> command_sequence;

		static std::shared_ptr<const AnimationSequence> LoadFromROM(const SpinballROM& src_rom, Uint32 offset, Ptr32 sprite_table_offset);
	};

	struct AnimationTable
	{
		std::vector<std::shared_ptr<const AnimationSequence>> animations;
	};
}