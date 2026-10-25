#pragma once

#include "types/rom_ptr.h"
#include "nlohmann/json_fwd.hpp"

namespace spintool::rom
{
	struct AnimObjectDefinition;
}

namespace spintool::editor
{
	struct AnimObjectDefinition
	{
		rom::Ptr32 rom_offset = 0; // Identifies the definition when referenced by game objects
		rom::Ptr32 sprite_table = 0;
		rom::Ptr32 starting_animation = 0;
		Uint8 palette_index = 0;
		Uint8 frame_time_ticks = 0;
		Uint8 unknown_1 = 0;
		Uint8 unknown_2 = 0;

		[[nodiscard]] nlohmann::json SerialiseToJSON() const;
		[[nodiscard]] static AnimObjectDefinition DeserialiseFromJSON(const nlohmann::json& anim_obj_context);
		[[nodiscard]] static AnimObjectDefinition FromROM(const rom::AnimObjectDefinition& rom_anim_obj);
	};
}
