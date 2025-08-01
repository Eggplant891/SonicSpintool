#pragma once

#include "nlohmann/json_fwd.hpp"

#include <string>
#include <memory>

namespace spintool::rom
{
	class TileBrush;
}

namespace spintool
{
	struct CustomTileBrush
	{
		nlohmann::json SerialiseToJSON() const;
		static CustomTileBrush DeserialiseFromJSON(const nlohmann::json& brush_context);
		std::unique_ptr<rom::TileBrush> tile_brush;
	};
}