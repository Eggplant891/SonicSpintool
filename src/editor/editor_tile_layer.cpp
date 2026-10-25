#include "editor/editor_tile_layer.h"

#include "nlohmann/json.hpp"

namespace spintool::editor
{
	nlohmann::json TileLayer::SerialiseToJSON() const
	{
		nlohmann::json tile_layer_json;
		tile_layer_json["tileset"] = tileset;
		tile_layer_json["tile_layout"] = tile_layout;
		tile_layer_json["tile_brushes"] = tile_brushes;

		return tile_layer_json;
	}

	TileLayer TileLayer::DeserialiseFromJSON(const nlohmann::json& tile_layer_context)
	{
		TileLayer tile_layer;
		tile_layer.tileset = tile_layer_context.at("tileset");
		tile_layer.tile_layout = tile_layer_context.at("tile_layout");
		tile_layer.tile_brushes = tile_layer_context.at("tile_brushes");

		return tile_layer;
	}
}
