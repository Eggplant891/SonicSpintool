#include "editor/editor_brush.h"
#include "nlohmann/json.hpp"
#include "rom/tile_brush.h"

namespace spintool
{
	nlohmann::json SerialiseTileinstance(rom::TileInstance& tile_instance)
	{
		nlohmann::json tile_instance_json;
		tile_instance_json["id"] = tile_instance.tile_index;
		tile_instance_json["palette_line"] = tile_instance.palette_line;
		tile_instance_json["hi_priority"] = tile_instance.is_high_priority;
		tile_instance_json["flip_v"] = tile_instance.is_flipped_vertically;
		tile_instance_json["flip_h"] = tile_instance.is_flipped_horizontally;

		return tile_instance_json;
	}

	rom::TileInstance DeserialiseTileinstance(const nlohmann::json& tile_context)
	{
		rom::TileInstance tile_instance;
		tile_instance.tile_index = tile_context["id"];
		tile_instance.palette_line = tile_context["palette_line"];
		tile_instance.is_high_priority = tile_context["hi_priority"];
		tile_instance.is_flipped_vertically = tile_context["flip_v"];
		tile_instance.is_flipped_horizontally = tile_context["flip_h"];

		return tile_instance;
	}

	nlohmann::json CustomTileBrush::SerialiseToJSON() const
	{
		nlohmann::json tiles = nlohmann::json::array();
		for (size_t y = 0; y < tile_brush->BrushHeight(); ++y)
		{
			nlohmann::json new_row = nlohmann::json::array();
			for (size_t x = 0; x < tile_brush->BrushWidth(); ++x)
			{
				new_row.emplace_back(SerialiseTileinstance(tile_brush->tiles[(y * tile_brush->BrushWidth()) + x]));
			}
			tiles.emplace_back(new_row);
		}
		return tiles;
	}

	CustomTileBrush CustomTileBrush::DeserialiseFromJSON(const nlohmann::json& brush_context)
	{
		nlohmann::json tiles = brush_context;
		CustomTileBrush new_brush;
		new_brush.tile_brush = std::make_unique<rom::TileBrush>(static_cast<Uint32>(tiles[0].size()), static_cast<Uint32>(tiles.size()));

		for (size_t y = 0; y < tiles.size(); ++y)
		{
			nlohmann::json row = tiles[y];
			for (size_t x = 0; x < row.size(); ++x)
			{
				new_brush.tile_brush->tiles.emplace_back(DeserialiseTileinstance(row[x]));
			}
		}

		return new_brush;
	}
}