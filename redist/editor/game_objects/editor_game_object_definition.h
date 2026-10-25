#pragma once

#include "editor/editor_collision_spline.h"
#include "types/rom_ptr.h"
#include "nlohmann/json_fwd.hpp"

#include <optional>

namespace spintool::rom
{
	struct GameObjectDefinition;
}

namespace spintool::editor
{
	struct GameObjectInstance
	{
		Uint8 type_id = 0;
		Uint8 instance_id = 0;
		Uint8 unk_1 = 0;
		Uint8 unk_2 = 0;
		Uint16 x_pos = 0;
		Uint16 y_pos = 0;
		Uint8 collision_width = 0;
		Uint8 collision_height = 0;
		rom::Ptr32 animation_definition = 0; // rom_offset of an entry in Level::m_anim_obj_definitions
		std::optional<CollisionSpline> collision;
		Uint16 flags = 0;
		bool flip_x = false;
		bool flip_y = false;

		[[nodiscard]] nlohmann::json SerialiseToJSON() const;
		[[nodiscard]] static GameObjectInstance DeserialiseFromJSON(const nlohmann::json& game_obj_context);
		[[nodiscard]] static GameObjectInstance FromROM(const rom::GameObjectDefinition& rom_game_obj);
	};
}
