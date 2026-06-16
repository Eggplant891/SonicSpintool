#include "rom/tile_layout.h"

#include "rom/spinball_rom.h"
#include "rom/tile.h"
#include "rom/tile_brush.h"

#include <memory>

namespace spintool::rom
{
	void TileLayout::BlitTileInstancesFromBrushInstances()
	{
		if (tile_brushes.empty() == false && layout_width > 0 && tile_brushes.front() != nullptr)
		{
			tile_instances.resize(tile_brush_instances.size() * tile_brushes.front()->TotalTiles());
			const size_t brush_width = tile_brushes.front()->BrushWidth();
			const size_t brush_height = tile_brushes.front()->BrushHeight();
			for (size_t i = 0; i < tile_brush_instances.size(); ++i)
			{
				const rom::TileBrushInstance& brush_instance = tile_brush_instances[i];
				if (brush_instance.tile_brush_index >= tile_brushes.size())
				{
					continue;
				}
				const std::unique_ptr<rom::TileBrush>& brush_def = tile_brushes[brush_instance.tile_brush_index];
				if (brush_def == nullptr)
				{
					continue;
				}
				const size_t brush_x_index = (i % layout_width) * brush_width;
				const size_t brush_y_index = (i / static_cast<size_t>(layout_width)) * brush_height;
				BlitTileBrushToLayout(*brush_def, brush_x_index, brush_y_index, brush_instance.is_flipped_horizontally, brush_instance.is_flipped_vertically);
			}
		}
	}

	void TileLayout::BlitTileBrushToLayout(const rom::TileBrush& brush, size_t x_tile_grid, size_t y_tile_grid, bool flip_x, bool flip_y)
	{
		const std::vector<rom::TileInstance> tiles_flipped = brush.TilesFlipped(flip_x, flip_y);

		for (size_t x = 0; x < brush.BrushWidth(); ++x)
		{
			for (size_t y = 0; y < brush.BrushHeight(); ++y)
			{
				size_t x_brush_index = x;
				size_t y_brush_index = y;
				const size_t source_brush_tile_index = (y_brush_index * brush.BrushWidth()) + x_brush_index;
				const size_t destination_index = ((y_tile_grid * layout_width * brush.BrushWidth()) + (y_brush_index * layout_width * brush.BrushWidth())) + (x_tile_grid + x_brush_index);

				if (source_brush_tile_index < tiles_flipped.size() && destination_index < tile_instances.size())
				{
					rom::TileInstance new_tile_instance = tiles_flipped[source_brush_tile_index];
					tile_instances[destination_index] = std::move(new_tile_instance);
				}
			}
		}
	}

	std::shared_ptr<spintool::rom::TileLayout> TileLayout::LoadFromROM(const SpinballROM& src_rom, Uint32 layout_width, Uint32 brushes_offset, Uint32 brushes_end, Uint32 layout_offset, std::optional<Uint32> layout_end)
	{
		const size_t rom_size = src_rom.m_buffer.size();
		if (!layout_end.has_value() || layout_width == 0 ||
			brushes_offset > brushes_end || brushes_end > rom_size ||
			layout_offset > *layout_end || *layout_end > rom_size)
		{
			return nullptr;
		}

		constexpr size_t brush_bytes = TileBrush::s_default_total_tiles * 2;
		const size_t brush_range = static_cast<size_t>(brushes_end - brushes_offset);
		const size_t layout_range = static_cast<size_t>(*layout_end - layout_offset);
		if (brush_range < brush_bytes || (brush_range % brush_bytes) != 0 || (layout_range % 2) != 0)
		{
			return nullptr;
		}

		auto new_layout = std::make_shared<TileLayout>();
		const size_t total_brushes = brush_range / brush_bytes;
		if (total_brushes > 0x400)
		{
			return nullptr;
		}
		new_layout->tile_brushes.resize(total_brushes);
		size_t current_offset = brushes_offset;

		for (std::unique_ptr<TileBrush>& current_brush : new_layout->tile_brushes)
		{
			current_brush = std::make_unique<TileBrush>(TileBrush::s_default_brush_width, TileBrush::s_default_brush_height);
			for (size_t i = 0; i < current_brush->TotalTiles(); ++i)
			{
				if (current_offset + 2 > brushes_end) return nullptr;
				const Uint8 first_byte = src_rom.m_buffer[current_offset++];
				const Uint8 second_byte = src_rom.m_buffer[current_offset++];
				TileInstance& t = current_brush->tiles.emplace_back();
				t.is_high_priority = (0x80 & first_byte) != 0;
				t.palette_line = ((0x40 | 0x20) & first_byte) >> 5;
				t.is_flipped_vertically = (0x10 & first_byte) != 0;
				t.is_flipped_horizontally = (0x08 & first_byte) != 0;
				t.tile_index = (static_cast<Uint16>(first_byte & 0x07) << 8) | second_byte;
			}
		}

		for (size_t off = layout_offset; off + 1 < *layout_end; off += 2)
		{
			const Uint8 first_byte = src_rom.m_buffer[off];
			const Uint8 second_byte = src_rom.m_buffer[off + 1];
			TileBrushInstance instance;
			instance.is_flipped_vertically = (0x10 & first_byte) != 0;
			instance.is_flipped_horizontally = (0x08 & first_byte) != 0;
			instance.tile_brush_index = (static_cast<Uint16>(first_byte & 0x03) << 8) | second_byte;
			new_layout->tile_brush_instances.emplace_back(instance);
		}

		new_layout->layout_width = static_cast<int>(layout_width);
		new_layout->layout_height = static_cast<int>(new_layout->tile_brush_instances.size() / layout_width);
		new_layout->BlitTileInstancesFromBrushInstances();
		new_layout->rom_data.SetROMData(layout_offset, *layout_end);
		return new_layout;
	}

	std::shared_ptr<spintool::rom::TileLayout> TileLayout::LoadFromROM(const SpinballROM& src_rom, const rom::TileSet& tileset, Uint32 layout_offset, std::optional<Uint32> layout_end)
	{
		const size_t rom_size = src_rom.m_buffer.size();
		if (layout_offset > rom_size || rom_size - layout_offset < 4)
		{
			return nullptr;
		}

		const Uint16 width = src_rom.ReadUint16(layout_offset);
		const Uint16 height = src_rom.ReadUint16(layout_offset + 2);
		if (width == 0 || height == 0 || width > 2048 || height > 2048)
		{
			return nullptr;
		}

		const uint64_t expected_end64 = static_cast<uint64_t>(layout_offset) + 4ULL + (static_cast<uint64_t>(width) * height * 2ULL);
		const size_t end_address = layout_end.has_value() ? *layout_end : static_cast<size_t>(expected_end64);
		if (end_address > rom_size || end_address < layout_offset + 4 || ((end_address - (layout_offset + 4)) % 2) != 0)
		{
			return nullptr;
		}

		auto new_layout = std::make_shared<TileLayout>();
		const Uint32 total_brushes = std::min<Uint32>(tileset.num_tiles, static_cast<Uint32>(tileset.tiles.size()));
		new_layout->tile_brushes.resize(total_brushes);
		for (Uint32 tile = 0; tile < total_brushes; ++tile)
		{
			new_layout->tile_brushes[tile] = std::make_unique<TileBrush>(1, 1);
			new_layout->tile_brushes[tile]->tiles.emplace_back().tile_index = static_cast<int>(tile);
		}

		new_layout->layout_width = width;
		new_layout->layout_height = height;
		for (size_t off = layout_offset + 4; off + 1 < end_address; off += 2)
		{
			const Uint8 first_byte = src_rom.m_buffer[off];
			const Uint8 second_byte = src_rom.m_buffer[off + 1];
			TileBrushInstance instance;
			instance.palette_line = ((0x40 | 0x20) & first_byte) >> 5;
			instance.is_flipped_vertically = (0x10 & first_byte) != 0;
			instance.is_flipped_horizontally = (0x08 & first_byte) != 0;
			instance.tile_brush_index = (static_cast<Uint16>(first_byte & 0x07) << 8) | second_byte;
			new_layout->tile_brush_instances.emplace_back(instance);
		}

		const size_t max_instances = static_cast<size_t>(width) * height;
		if (new_layout->tile_brush_instances.size() > max_instances)
			new_layout->tile_brush_instances.resize(max_instances);
		new_layout->BlitTileInstancesFromBrushInstances();
		new_layout->rom_data.SetROMData(layout_offset, static_cast<Uint32>(end_address));
		return new_layout;
	}

	std::shared_ptr<spintool::rom::TileLayout> TileLayout::LoadRawTilesFromROM(const SpinballROM& src_rom, const rom::TileSet& tileset, Uint32 layout_width, Uint32 layout_height, Uint32 layout_offset, Uint32 layout_end)
	{
		const size_t rom_size = src_rom.m_buffer.size();
		if (layout_width == 0 || layout_height == 0 || layout_width > 2048 || layout_height > 2048 ||
			layout_offset > layout_end || layout_end > rom_size)
		{
			return nullptr;
		}

		const uint64_t expected_bytes = static_cast<uint64_t>(layout_width) * layout_height * 2ULL;
		if (expected_bytes != static_cast<uint64_t>(layout_end - layout_offset))
		{
			return nullptr;
		}

		auto new_layout = std::make_shared<TileLayout>();
		const Uint32 total_brushes = std::min<Uint32>(tileset.num_tiles, static_cast<Uint32>(tileset.tiles.size()));
		new_layout->tile_brushes.resize(total_brushes);
		for (Uint32 tile = 0; tile < total_brushes; ++tile)
		{
			new_layout->tile_brushes[tile] = std::make_unique<TileBrush>(1, 1);
			new_layout->tile_brushes[tile]->tiles.emplace_back().tile_index = static_cast<int>(tile);
		}

		new_layout->layout_width = static_cast<int>(layout_width);
		new_layout->layout_height = static_cast<int>(layout_height);
		for (size_t off = layout_offset; off + 1 < layout_end; off += 2)
		{
			const Uint8 first_byte = src_rom.m_buffer[off];
			const Uint8 second_byte = src_rom.m_buffer[off + 1];
			TileBrushInstance instance;
			instance.palette_line = ((0x40 | 0x20) & first_byte) >> 5;
			instance.is_flipped_vertically = (0x10 & first_byte) != 0;
			instance.is_flipped_horizontally = (0x08 & first_byte) != 0;
			instance.tile_brush_index = (static_cast<Uint16>(first_byte & 0x07) << 8) | second_byte;
			new_layout->tile_brush_instances.emplace_back(instance);
		}

		new_layout->BlitTileInstancesFromBrushInstances();
		new_layout->rom_data.SetROMData(layout_offset, layout_end);
		return new_layout;
	}

	void TileLayout::CollapseTilesIntoBrushes(const rom::TileSet& tile_set)
	{
		if (layout_width <= 0 || tile_instances.empty()) return;
		const size_t num_brush_instances = tile_instances.size() / TileBrush::s_default_total_tiles;
		const size_t previous_num_brushes = tile_brushes.size();

		std::vector<std::unique_ptr<TileBrush>> candidate_brushes;
		candidate_brushes.reserve(num_brush_instances);
		for (size_t i = 0; i < num_brush_instances; ++i)
		{
			std::unique_ptr<TileBrush> next_brush = std::make_unique<TileBrush>(TileBrush::s_default_brush_width, TileBrush::s_default_brush_height);
			const Uint32 brush_width = next_brush->BrushWidth();
			const Uint32 brush_height = next_brush->BrushHeight();
			next_brush->tiles.resize(next_brush->TotalTiles());
			for (Uint32 x = 0; x < brush_width; ++x)
			{
				for (Uint32 y = 0; y < brush_height; ++y)
				{
					const Point brush_coords = LinearIndexToGridCoordinates(i) * static_cast<float>(brush_width);
					const size_t destination_index = ((brush_coords.y * layout_width * brush_width) + (y * layout_width * brush_height)) + (brush_coords.x + x);
					const size_t brush_destination_index = next_brush->GridCoordinatesToLinearIndex(Point{ static_cast<int>(x), static_cast<int>(y) });
					if (brush_destination_index < next_brush->tiles.size() && destination_index < tile_instances.size())
					{
						next_brush->tiles[brush_destination_index] = tile_instances[destination_index];
					}
				}
			}
			candidate_brushes.emplace_back(std::move(next_brush));
		}

		std::vector<std::unique_ptr<TileBrush>> final_brush_set;
		std::vector<std::unique_ptr<TileBrush>> final_brush_set_x;
		std::vector<std::unique_ptr<TileBrush>> final_brush_set_y;
		std::vector<std::unique_ptr<TileBrush>> final_brush_set_xy;

		std::vector<rom::TileBrushInstance> brush_instances;

		CacheBrushSymmetryFlags(*this, tile_set);

		for (std::unique_ptr<TileBrush>& candidate_brush : candidate_brushes)
		{
			bool found_match = false;
			rom::TileBrushInstance new_instance{};

			// For each candidate brush, check if there are any matches in the existing list of brushes. Ensure to check unflipped, and flipx/y and both
			if (final_brush_set.empty() == false)
			{
				for (size_t i = 0; i < final_brush_set.size(); ++i)
				{
					std::unique_ptr<TileBrush>& brush_def = final_brush_set[i];

					// If candidate brush exists, push Brush instance references the existing brush, and any flip flags
					if (candidate_brush->IsBrushSymmetricallyEqualTo(*final_brush_set[i], tile_set, false, false))
					{
						new_instance.tile_brush_index = static_cast<Uint16>(i);
						found_match = true;
						break;
					}

					if (candidate_brush->IsBrushSymmetricallyEqualTo(*final_brush_set_xy[i], tile_set, true, true))
					{
						new_instance.tile_brush_index = static_cast<Uint16>(i);
						new_instance.is_flipped_horizontally = true;
						new_instance.is_flipped_vertically = true;
						found_match = true;
						break;
					}

					if (candidate_brush->IsBrushSymmetricallyEqualTo(*final_brush_set_x[i], tile_set, true, false))
					{
						new_instance.tile_brush_index = static_cast<Uint16>(i);
						new_instance.is_flipped_horizontally = true;
						found_match = true;
						break;
					}

					if (candidate_brush->IsBrushSymmetricallyEqualTo(*final_brush_set_y[i], tile_set, false, true))
					{
						new_instance.tile_brush_index = static_cast<Uint16>(i);
						new_instance.is_flipped_vertically = true;
						found_match = true;
						break;
					}
				}
			}

			// If candidate brush does not exist, create a brush and push tothe final brush list
			if (found_match == false)
			{
				auto new_brush = std::make_unique<TileBrush>(*candidate_brush);
				auto new_brush_x = std::make_unique<TileBrush>(*candidate_brush);
				auto new_brush_y = std::make_unique<TileBrush>(*candidate_brush);
				auto new_brush_xy = std::make_unique<TileBrush>(*candidate_brush);

				new_brush_x->tiles = new_brush->TilesFlipped(true, false);
				new_brush_y->tiles = new_brush->TilesFlipped(false, true);
				new_brush_xy->tiles = new_brush->TilesFlipped(true, true);

				assert(new_brush->tiles == new_brush_x->TilesFlipped(true, false));
				assert(new_brush->tiles == new_brush_y->TilesFlipped(false, true));
				assert(new_brush->tiles == new_brush_xy->TilesFlipped(true, true));

				final_brush_set.emplace_back(std::move(new_brush));
				final_brush_set_x.emplace_back(std::move(new_brush_x));
				final_brush_set_y.emplace_back(std::move(new_brush_y));
				final_brush_set_xy.emplace_back(std::move(new_brush_xy));
				new_instance.tile_brush_index = static_cast<Uint16>(final_brush_set.size() - 1);
			}

			brush_instances.emplace_back(std::move(new_instance));
		}

		//assert(tile_brushes == final_brush_set);
		//assert(tile_brush_instances == brush_instances);

		tile_brushes = std::move(final_brush_set);
		tile_brush_instances = std::move(brush_instances);

		CacheBrushSymmetryFlags(*this, tile_set);
	}

	void TileLayout::SaveToROM(SpinballROM& src_rom, const rom::TileSet& tile_set, Uint32 brushes_offset, Uint32 layout_offset)
	{
		Uint32 current_offset = brushes_offset;

		CollapseTilesIntoBrushes(tile_set);

		for (std::unique_ptr<TileBrush>& current_brush : tile_brushes)
		{
			for (const TileInstance& tile : current_brush->tiles)
			{
				Uint8 first_byte = 0;
				Uint8 second_byte = 0;


				first_byte |= tile.is_high_priority ? 0x80 : 0x00;
				first_byte |= (0x40 | 0x20) & (tile.palette_line << 5);
				first_byte |= tile.is_flipped_vertically ? 0x10 : 0x00;
				first_byte |= tile.is_flipped_horizontally ? 0x08 : 0x00;
				first_byte |= ((tile.tile_index) & static_cast<Uint16>((0x0100 | 0x0200 | 0x0400))) >> 8;

				current_offset = src_rom.WriteUint8(current_offset, first_byte);
				current_offset = src_rom.WriteUint8(current_offset, tile.tile_index & 0x00FF);

			}
		}

		current_offset = layout_offset;
		for (TileBrushInstance& brush_instance : tile_brush_instances)
		{
			//brush_instance.is_high_priority = (0x80 & first_byte) != 0;
			Uint8 first_byte = 0;

			//first_byte |= (brush_instance.palette_line << 5) & (0x40 | 0x20);
			first_byte |= brush_instance.is_flipped_vertically ? 0x10 : 0x00;
			first_byte |= brush_instance.is_flipped_horizontally ? 0x08 : 0x00;
			first_byte |= ((brush_instance.tile_brush_index) & static_cast<Uint16>((0x0100 | 0x0200 | 0x0400))) >> 8;

			current_offset = src_rom.WriteUint8(current_offset, first_byte);
			current_offset = src_rom.WriteUint8(current_offset, brush_instance.tile_brush_index & 0x00FF);
		}
	}

	size_t TileLayout::GridCoordinatesToLinearIndex(Point grid_coord) const
	{
		return grid_coord.x + (grid_coord.y * layout_width);
	}

	Point TileLayout::LinearIndexToGridCoordinates(size_t linear_index) const
	{
		Point out_coord{};
		out_coord.x = (linear_index % layout_width);
		out_coord.y = static_cast<int>(((linear_index - (linear_index % layout_width)) / static_cast<float>(layout_width)));

		return out_coord;
	}

	void TileLayout::CacheBrushSymmetryFlags(TileLayout& tile_layout, const TileSet& tile_set)
	{
		// Verify symmetrical tile brushes
		for (std::unique_ptr<rom::TileBrush>& current_brush : tile_layout.tile_brushes)
		{
			current_brush->CacheSymmetryFlags(tile_set);
		}
	}
}
