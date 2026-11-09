#pragma once

#include "rom_data.h"

#include <vector>
#include <memory>
#include <optional>
#include "ui/ui_sprite.h"
#include "types/rom_ptr.h"

namespace spintool::rom
{
	class SpinballROM;
	struct Sprite;

	// Matches the game's interpreter (AnimObj_UpdateTemporaries / AnimObj_UpdateAnimation /
	// AnimObj_DecodeCurrentAnimFrame / AnimObj_UpdateStateMachine).
	//
	// An animation is a stream of 16-bit words. A word with bit 15 clear is a frame:
	//   bits 0-13: sprite index, bit 14: flip X (XORed with the object's flip). $3FFF hides the sprite.
	// A word with bit 15 set is a command:
	//   bits 13-14: operand mode (see AnimationOperandMode)
	//   bits 8-12:  command type ($1F = extended: the low byte is the type and a 16-bit word follows)
	//   bits 0-7:   argument
	enum class AnimationCommandType
	{
		KILL_OBJECT = 0x00,                 // Marks the object as finished
		PAUSE = 0x01,                       // Clears the frame timers; resumes when the object is restarted
		NOP = 0x02,
		GOTO = 0x03,                        // Continues at the pointer
		LOOP = 0x04,                        // ++counter < argument: go to the pointer, else reset the counter
		SWITCH = 0x06,                      // N, then N+1 pointers. Operand 1..N+1 picks one; otherwise falls through
		HOLD_FRAME = 0x07,                  // Keeps the current frame for operand - 1 more ticks
		HOLD_FRAME_RANDOM = 0x08,           // Keeps the current frame for rand(operand) ticks
		WAIT_FOR_FLAG = 0x09,               // Stops until another routine sets the object's wake flag
		SET_ANIM_TICKS_PER_FRAME = 0x0B,
		CREATE_ANIM_OBJ_INSTANCE = 0x0C,    // Pointer to an animated object definition, then X and Y offsets
		COMPUTED_FRAME = 0x0D,              // A frame chosen by a variable: base word + variable, or word_table[variable]
		CLEAR_VARIABLE = 0x0E,              // variable[operand] = 0
		SET_VARIABLE = 0x0F,                // variable[argument] = following word
		EXECUTE_AS_FUNCTION_PTR = 0x10,
		PLAY_EXCLUSIVE_AUDIO = 0x11,
		LOAD_SPRITE_TILES = 0x13,
		SET_HORIZONTAL_SPEED = 0x14,        // Object field $0C and the sprite's field +4
		STOP_SONG = 0x15,
		ADD_X_OFFSET = 0x20,                // Extended commands from here on
		ADD_Y_OFFSET = 0x21,
		SET_X_OFFSET = 0x25,
		SET_Y_OFFSET = 0x26,

		FRAME = 0x100,                      // Not a command: a sprite frame
		HIDDEN_FRAME = 0x101,               // Not a command: frame word $3FFF
		INVALID = -2,                       // The game locks up (WaitForIRQ) on these
		UNKNOWN = -1,
	};
	const char* AnimationCommandTypeToString(AnimationCommandType command_type);

	enum class AnimationOperandMode
	{
		INLINE = 0,         // %00: the argument (sign-extended byte), or the extended word
		VARIABLE = 1,       // %01: variable[argument]
		POINTER = 2,        // %10: a 32-bit pointer follows; the operand is variable[argument]
		TABLE_LOOKUP = 3,   // %11: a 32-bit pointer follows; the operand is table[variable[argument]]
	};
	const char* AnimationOperandModeToString(AnimationOperandMode mode);

	struct AnimationCommand
	{
		ROMData rom_data;
		AnimationCommandType command_type = AnimationCommandType::UNKNOWN;
		AnimationOperandMode operand_mode = AnimationOperandMode::INLINE;
		std::shared_ptr<UISpriteTexture> ui_frame_sprite;

		Uint32 command_data = 0;              // Frames: sprite index. Pointer modes: the pointer. Otherwise: the inline value.
		Uint8 command_data_extra_in_word = 0; // The argument byte (or the low byte of the extended word)
		Uint8 data_size = 1;                  // Size of command_data when displayed: 1, 2 or 4 bytes
		Sint32 inline_value = 0;              // The decoded inline operand (signed for normal commands)
		Uint16 argument = 0;                  // The argument (the full word for extended commands)
		Uint32 pointer = 0;                   // Pointer operand (28 bits), when operand_mode is POINTER or TABLE_LOOKUP
		Uint8 table_element_mode = 0;         // TABLE_LOOKUP: the pointer's top nibble selects the table's element format
		std::vector<Uint32> jump_targets;     // GOTO / LOOP / SWITCH destinations
		Sint16 extra_x = 0;                   // CREATE_ANIM_OBJ_INSTANCE: X offset
		Sint16 extra_y = 0;                   // CREATE_ANIM_OBJ_INSTANCE: Y offset
		Uint16 extra_word = 0;                // SET_VARIABLE value, or COMPUTED_FRAME base frame

		bool flip_x = false;
		bool flip_y = false;

		[[nodiscard]] bool IsFrame() const { return command_type == AnimationCommandType::FRAME || command_type == AnimationCommandType::HIDDEN_FRAME; }
		[[nodiscard]] bool UsesVariableOperand() const { return operand_mode != AnimationOperandMode::INLINE; }
	};

	struct AnimationSequence
	{
		ROMData rom_data;
		// Every command reachable from the start, in ROM order. Use FindCommandIndex to follow the control flow.
		std::vector<AnimationCommand> command_sequence;
		size_t start_command_index = 0;

		Point min_dynamic_offset{ 0,0 };
		Point max_dynamic_offset{ 0,0 };
		// SET_X/Y_OFFSET write the object's absolute level position. The preview measures them from the first one in the
		// animation, so it shows the movement between them rather than the position in the level.
		std::optional<Sint32> set_x_origin;
		std::optional<Sint32> set_y_origin;

		[[nodiscard]] std::optional<size_t> FindCommandIndex(Uint32 rom_offset) const;

		static std::shared_ptr<const AnimationSequence> LoadFromROM(const SpinballROM& src_rom, Uint32 offset, Ptr32 sprite_table_offset);
		static AnimationCommand DecodeCommand(const SpinballROM& src_rom, Uint32 offset);
	};

	struct AnimationTable
	{
		std::vector<std::shared_ptr<const AnimationSequence>> animations;
	};
}
