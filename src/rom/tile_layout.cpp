#include "rom/tile_layout.h"

#include "rom/spinball_rom.h"

namespace spintool::rom
{
	bool operator==(const TileInstance& lhs, const TileInstance& rhs)
	{
		return lhs.is_flipped_horizontally == rhs.is_flipped_horizontally
			&& lhs.is_flipped_vertically == rhs.is_flipped_vertically
			&& lhs.is_high_priority == rhs.is_high_priority
			&& lhs.palette_line == rhs.palette_line
			&& lhs.tile_index == rhs.tile_index;
	}

	bool operator==(const std::unique_ptr<TileInstance>& lhs, const std::unique_ptr<TileInstance>& rhs)
	{
		return lhs != nullptr && rhs != nullptr && *lhs == *rhs;
	}

	bool operator==(const TileBrushBase& lhs, const TileBrushBase& rhs)
	{
		return lhs.BrushWidth() == rhs.BrushWidth()
			&& lhs.BrushHeight() == rhs.BrushHeight()
			&& lhs.tiles == rhs.tiles;
	}
	bool operator==(const std::unique_ptr<TileBrushBase>& lhs, const std::unique_ptr<TileBrushBase>& rhs)
	{
		return lhs != nullptr && rhs != nullptr && *lhs == *rhs;
	}

	bool operator==(const TileBrushInstance& lhs, const TileBrushInstance& rhs)
	{
		return lhs.is_flipped_horizontally == rhs.is_flipped_horizontally
			&& lhs.is_flipped_vertically == rhs.is_flipped_vertically
			&& lhs.palette_line == rhs.palette_line
			&& lhs.tile_brush_index == rhs.tile_brush_index;
	}

	bool operator==(const std::unique_ptr<TileBrushInstance>& lhs, const std::unique_ptr<TileBrushInstance>& rhs)
	{
		return lhs != nullptr && rhs != nullptr && *lhs == *rhs;
	}

	std::shared_ptr<spintool::rom::TileLayout> TileLayout::LoadFromROM(const SpinballROM& src_rom, Uint32 layout_width, Uint32 brushes_offset, Uint32 brushes_end, Uint32 layout_offset, std::optional<Uint32> layout_end)
	{
		const Uint8* start_byte = &src_rom.m_buffer[brushes_offset];
		const Uint8* current_byte = start_byte;

		if (layout_end.has_value() == false)
		{
			return nullptr;
		}

		auto new_layout = std::make_shared<TileLayout>();

		const size_t total_brushes = (brushes_end - brushes_offset) / (TileBrush<4,4>::s_brush_total_tiles*2);
		new_layout->tile_brushes.resize(total_brushes);

		for (std::unique_ptr<TileBrushBase>& current_brush : new_layout->tile_brushes)
		{
			current_brush = std::make_unique<TileBrush<4, 4>>();
			for (size_t i = 0; i < current_brush->TotalTiles(); ++i)
			{
				const Uint8 first_byte = *current_byte;
				++current_byte;
				const Uint8 second_byte = *current_byte;
				++current_byte;

				TileInstance& new_tile_instance = current_brush->tiles.emplace_back();
				new_tile_instance.is_high_priority = (0x80 & first_byte) != 0;
				new_tile_instance.palette_line = ((0x40 | 0x20) & first_byte) >> 5;
				new_tile_instance.is_flipped_vertically = (0x10 & first_byte) != 0;
				new_tile_instance.is_flipped_horizontally = (0x08 & first_byte) != 0;
				new_tile_instance.tile_index = (static_cast<Uint16>((0x01 | 0x02 | 0x04) & first_byte) << 8) | second_byte;
			}
		}

		current_byte = &src_rom.m_buffer[layout_offset];

		for (size_t i = 0; i < (*layout_end - layout_offset) / 2; ++i)
		{
			const Uint8 first_byte = *current_byte;
			++current_byte;
			const Uint8 second_byte = *current_byte;
			++current_byte;

			TileBrushInstance brush_instance;
			brush_instance.is_flipped_vertically = (0x10 & first_byte) != 0;
			brush_instance.is_flipped_horizontally = (0x08 & first_byte) != 0;
			static Uint8 first_byte_mask = 0x03;
			static Uint8 second_byte_mask = 0xFF;
			brush_instance.tile_brush_index = (static_cast<Uint16>(first_byte & 0x03) << 8) | static_cast<Uint16>(second_byte & 0xFF);
			new_layout->tile_brush_instances.emplace_back(brush_instance);
		}

		new_layout->layout_width = layout_width;
		new_layout->layout_height = static_cast<Uint32>(new_layout->tile_brush_instances.size()) / layout_width;

		if (new_layout->tile_brushes.empty() == false)
		{
			new_layout->tile_instances.resize(new_layout->tile_brush_instances.size() * new_layout->tile_brushes.front()->TotalTiles());
			const size_t brush_width = new_layout->tile_brushes.front()->BrushWidth();
			const size_t brush_height = new_layout->tile_brushes.front()->BrushHeight();
			for (size_t i = 0; i < new_layout->tile_brush_instances.size(); ++i)
			{
				const rom::TileBrushInstance& brush_instance = new_layout->tile_brush_instances[i];
				if (brush_instance.tile_brush_index >= new_layout->tile_brushes.size())
				{
					continue;
				}
				const std::unique_ptr<rom::TileBrushBase>& brush_def = new_layout->tile_brushes[brush_instance.tile_brush_index];
				const std::vector<rom::TileInstance> tiles_flipped = brush_def->TilesFlipped(brush_instance.is_flipped_horizontally, brush_instance.is_flipped_vertically);

				for (size_t x = 0; x < brush_width; ++x)
				{
					for (size_t y = 0; y < brush_height; ++y)
					{
						size_t x_brush_index = x;
						size_t y_brush_index = y;
						const size_t source_brush_tile_index = (y_brush_index * brush_width) + x_brush_index;
						const size_t brush_x_index = (i % new_layout->layout_width) * brush_width;
						const size_t brush_y_index = static_cast<int>(((i - (i % new_layout->layout_width)) / static_cast<float>(new_layout->layout_width))) * brush_width;
						const size_t destination_index = ((brush_y_index * new_layout->layout_width * brush_width) + (y_brush_index * new_layout->layout_width * brush_height)) + (brush_x_index + x_brush_index);

						rom::TileInstance new_tile_instance = tiles_flipped[source_brush_tile_index];
						new_layout->tile_instances[destination_index] = std::move(new_tile_instance);
					}
				}
			}
		}

		new_layout->rom_data.SetROMData(layout_offset, *layout_end);
		return new_layout;
	}

	std::shared_ptr<spintool::rom::TileLayout> TileLayout::LoadFromROM(const SpinballROM& src_rom, const rom::TileSet& tileset, Uint32 layout_offset, std::optional<Uint32> layout_end)
	{
		auto new_layout = std::make_shared<TileLayout>();

		const Uint32 total_brushes = tileset.num_tiles;
		new_layout->tile_brushes.resize(total_brushes);

		for (Uint32 tile = 0; tile < new_layout->tile_brushes.size(); ++tile)
		{
			new_layout->tile_brushes[tile] = std::make_unique<TileBrush<1, 1>>();
			TileInstance& new_tile_instance = new_layout->tile_brushes[tile]->tiles.emplace_back();
			new_tile_instance.tile_index = static_cast<int>(tile);
		}

		const Uint8* current_byte = &src_rom.m_buffer[layout_offset];
		const Uint16 width = src_rom.ReadUint16(layout_offset);
		const Uint16 height = src_rom.ReadUint16(layout_offset+2);
		current_byte += 4;

		new_layout->layout_width = width;
		new_layout->layout_height = height;

		const auto end_address = layout_end.value_or(layout_offset + 4 + (width * 2 * height));

		for (Uint32 i = 0; i < (end_address - layout_offset) / 2; ++i)
		{
			const Uint8 first_byte = *current_byte;
			++current_byte;
			const Uint8 second_byte = *current_byte;
			++current_byte;

			TileBrushInstance brush_instance;
			//brush_instance.is_high_priority = (0x80 & first_byte) != 0;
			brush_instance.palette_line = ((0x40 | 0x20) & first_byte) >> 5;
			brush_instance.is_flipped_vertically = (0x10 & first_byte) != 0;
			brush_instance.is_flipped_horizontally = (0x08 & first_byte) != 0;
			brush_instance.tile_brush_index = (static_cast<Uint16>((0x01 | 0x02 | 0x04) & first_byte) << 8) | second_byte;
			new_layout->tile_brush_instances.emplace_back(brush_instance);
		}

		if (new_layout->tile_brushes.empty() == false)
		{
			new_layout->tile_instances.resize(new_layout->tile_brush_instances.size() * new_layout->tile_brushes.front()->TotalTiles());
			const size_t brush_width = new_layout->tile_brushes.front()->BrushWidth();
			const size_t brush_height = new_layout->tile_brushes.front()->BrushHeight();
			for (size_t i = 0; i < new_layout->tile_brush_instances.size(); ++i)
			{
				const rom::TileBrushInstance& brush_instance = new_layout->tile_brush_instances[i];
				if (brush_instance.tile_brush_index >= new_layout->tile_brushes.size())
				{
					continue;
				}
				const std::unique_ptr<rom::TileBrushBase>& brush_def = new_layout->tile_brushes[brush_instance.tile_brush_index];
				const std::vector<rom::TileInstance> tiles_flipped = brush_def->TilesFlipped(brush_instance.is_flipped_horizontally, brush_instance.is_flipped_vertically);

				for (size_t x = 0; x < brush_width; ++x)
				{
					for (size_t y = 0; y < brush_height; ++y)
					{
						size_t x_brush_index = x;
						size_t y_brush_index = y;
						const size_t source_brush_tile_index = (y_brush_index * brush_width) + x_brush_index;
						const size_t brush_x_index = (i % new_layout->layout_width) * brush_width;
						const size_t brush_y_index = static_cast<int>(((i - (i % new_layout->layout_width)) / static_cast<float>(new_layout->layout_width))) * brush_width;
						const size_t destination_index = ((brush_y_index * new_layout->layout_width * brush_width) + (y_brush_index * new_layout->layout_width * brush_height)) + (brush_x_index + x_brush_index);

						rom::TileInstance new_tile_instance = tiles_flipped[source_brush_tile_index];
						if (brush_instance.is_flipped_horizontally)
						{
							new_tile_instance.is_flipped_horizontally = !new_tile_instance.is_flipped_horizontally;
						}

						if (brush_instance.is_flipped_vertically)
						{
							new_tile_instance.is_flipped_vertically = !new_tile_instance.is_flipped_vertically;
						}

						new_layout->tile_instances[destination_index] = std::move(new_tile_instance);
					}
				}
			}
		}

		new_layout->rom_data.SetROMData(layout_offset, end_address);

		return new_layout;
	}

	void TileLayout::CollapseTilesIntoBrushes()
	{
		const size_t brush_width = TileBrush<4, 4>::s_brush_width;
		const size_t brush_height = TileBrush<4, 4>::s_brush_height;
		const size_t num_brush_instances = tile_instances.size() / TileBrush<4, 4>::s_brush_total_tiles;
		const size_t previous_num_brushes = tile_brushes.size();

		std::vector<std::unique_ptr<TileBrushBase>> candidate_brushes;
		candidate_brushes.reserve(num_brush_instances);
		for (size_t i = 0; i < num_brush_instances; ++i)
		{
			std::unique_ptr<TileBrushBase> next_brush = std::make_unique<TileBrush<4, 4>>();
			next_brush->tiles.resize(next_brush->TotalTiles());
			for (size_t x = 0; x < brush_width; ++x)
			{
				for (size_t y = 0; y < brush_height; ++y)
				{
					size_t x_brush_index = x;
					size_t y_brush_index = y;
					const size_t brush_x_index = (i % layout_width) * brush_width;
					const size_t brush_y_index = static_cast<int>(((i - (i % layout_width)) / static_cast<float>(layout_width))) * brush_width;
					const size_t destination_index = ((brush_y_index * layout_width * brush_width) + (y_brush_index * layout_width * brush_height)) + (brush_x_index + x_brush_index);
					const size_t brush_destination_index = (y * brush_width) + x;
					next_brush->tiles[brush_destination_index] = tile_instances[destination_index];
				}
			}
			candidate_brushes.emplace_back(std::move(next_brush));
		}

		std::vector<std::unique_ptr<TileBrushBase>> final_brush_set;
		std::vector<std::unique_ptr<TileBrushBase>> final_brush_set_x;
		std::vector<std::unique_ptr<TileBrushBase>> final_brush_set_y;
		std::vector<std::unique_ptr<TileBrushBase>> final_brush_set_xy;

		std::vector<rom::TileBrushInstance> brush_instances;

		for (std::unique_ptr<TileBrushBase>& candidate_brush : candidate_brushes)
		{
			bool found_match = false;
			rom::TileBrushInstance new_instance{};

			// For each candidate brush, check if there are any matches in the existing list of brushes. Ensure to check unflipped, and flipx/y and both
			if (final_brush_set.empty() == false)
			{
				for (size_t i = 0; i < final_brush_set.size(); ++i)
				{
					std::unique_ptr<TileBrushBase>& brush_def = final_brush_set[i];

					// If candidate brush exists, push Brush instance references the existing brush, and any flip flags
					if (final_brush_set[i]->tiles == candidate_brush->tiles)
					{
						new_instance.tile_brush_index = static_cast<Uint16>(i);
						found_match = true;
						break;
					}

					if (final_brush_set_x[i]->tiles == candidate_brush->tiles)
					{
						new_instance.tile_brush_index = static_cast<Uint16>(i);
						new_instance.is_flipped_horizontally = true;
						found_match = true;
						break;
					}

					if (final_brush_set_y[i]->tiles == candidate_brush->tiles)
					{
						new_instance.tile_brush_index = static_cast<Uint16>(i);
						new_instance.is_flipped_vertically = true;
						found_match = true;
						break;
					}

					if (final_brush_set_xy[i]->tiles == candidate_brush->tiles)
					{
						new_instance.tile_brush_index = static_cast<Uint16>(i);
						new_instance.is_flipped_horizontally = true;
						new_instance.is_flipped_vertically = true;
						found_match = true;
						break;
					}
				}
			}

			// If candidate brush does not exist, create a brush and push tothe final brush list
			if (found_match == false)
			{
				auto new_brush = std::make_unique<TileBrush<4, 4>>(*static_cast<TileBrush<4, 4>*>(candidate_brush.get()));
				auto new_brush_x = std::make_unique<TileBrush<4, 4>>(*static_cast<TileBrush<4, 4>*>(candidate_brush.get()));
				auto new_brush_y = std::make_unique<TileBrush<4, 4>>(*static_cast<TileBrush<4, 4>*>(candidate_brush.get()));
				auto new_brush_xy = std::make_unique<TileBrush<4, 4>>(*static_cast<TileBrush<4, 4>*>(candidate_brush.get()));

				new_brush_x->tiles = new_brush->TilesFlipped(true, false);
				new_brush_y->tiles = new_brush->TilesFlipped(false, true);
				new_brush_xy->tiles = new_brush->TilesFlipped(true, true);

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
	}

	void TileLayout::SaveToROM(SpinballROM& src_rom, Uint32 brushes_offset, Uint32 layout_offset)
	{
		Uint32 current_offset = brushes_offset;

		CollapseTilesIntoBrushes();

		for (std::unique_ptr<TileBrushBase>& current_brush : tile_brushes)
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

	std::vector<spintool::rom::TileInstance> TileBrushBase::TilesFlipped(bool flip_x, bool flip_y) const
	{
		std::vector<spintool::rom::TileInstance> out_tiles = tiles;

		if (flip_x)
		{
			for (size_t x = 0; x < BrushWidth() / 2; ++x)
			{
				for (size_t y = 0; y < BrushHeight(); ++y)
				{
					const size_t x_offset_inverse = ((BrushWidth() - 1) - x);
					const size_t y_offset = y * BrushWidth();
					out_tiles[y_offset + x].is_flipped_horizontally = !out_tiles[y_offset + x].is_flipped_horizontally;
					out_tiles[y_offset + x_offset_inverse].is_flipped_horizontally = !out_tiles[y_offset + x_offset_inverse].is_flipped_horizontally;
					std::swap(out_tiles[y_offset + x], out_tiles[y_offset + x_offset_inverse]);
				}
			}
		}

		if (flip_y)
		{
			for (size_t x = 0; x < BrushWidth(); ++x)
			{
				for (size_t y = 0; y < BrushHeight() / 2; ++y)
				{
					const size_t y_offset = y * BrushWidth();
					const size_t y_offset_inverse = ((BrushHeight() - 1) - y) * BrushWidth();
					out_tiles[y_offset + x].is_flipped_vertically = !out_tiles[y_offset + x].is_flipped_vertically;
					out_tiles[y_offset_inverse + x].is_flipped_vertically = !out_tiles[y_offset_inverse + x].is_flipped_vertically;
					std::swap(out_tiles[y_offset + x], out_tiles[y_offset_inverse + x]);
				}
			}
		}

		return out_tiles;
	}
}