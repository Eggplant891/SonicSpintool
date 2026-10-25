#pragma once

#include "types/rom_ptr.h"
#include "nlohmann/json_fwd.hpp"

namespace spintool::editor
{
	// Tile layers are not yet converted into editor data, so only the ROM locations of their assets are kept.
	struct TileLayer
	{
		rom::Ptr32 tileset = 0;
		rom::Ptr32 tile_layout = 0;
		rom::Ptr32 tile_brushes = 0;

		[[nodiscard]] nlohmann::json SerialiseToJSON() const;
		[[nodiscard]] static TileLayer DeserialiseFromJSON(const nlohmann::json& tile_layer_context);
	};
}
