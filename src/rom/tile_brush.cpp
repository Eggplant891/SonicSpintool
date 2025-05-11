#include "rom/tile_brush.h"
#include "rom/tile.h"

#include "rom/sprite_tile.h"
#include "rom/tileset.h"
#include "rom/level.h"
#include "rom/sprite.h"

namespace spintool::rom
{
	bool operator==(const TileBrush& lhs, const TileBrush& rhs)
	{
		return lhs.BrushWidth() == rhs.BrushWidth()
			&& lhs.BrushHeight() == rhs.BrushHeight()
			&& lhs.tiles == rhs.tiles;
	}

	bool operator==(const std::unique_ptr<TileBrush>& lhs, const std::unique_ptr<TileBrush>& rhs)
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

	Uint32 TileBrush::BrushWidth() const
	{
		return m_brush_width;
	}

	Uint32 TileBrush::BrushHeight() const
	{
		return m_brush_height;
	}

	Uint32 TileBrush::TotalTiles() const
	{
		return BrushWidth() * BrushHeight();
	}

	std::vector<rom::TileInstance> TileBrush::TilesFlipped(bool flip_x, bool flip_y) const
	{
		std::vector<rom::TileInstance> out_tiles = tiles;

		if (flip_x)
		{
			for (size_t x = 0; x < ((BrushWidth() / 2) + (BrushWidth() % 2)); ++x)
			{
				for (size_t y = 0; y < BrushHeight(); ++y)
				{
					const size_t x_offset_inverse = ((BrushWidth() - 1) - x);
					const size_t y_offset = y * BrushWidth();
					rom::TileInstance& lhs = out_tiles[y_offset + x];
					rom::TileInstance& rhs = out_tiles[y_offset + x_offset_inverse];
					lhs.is_flipped_horizontally = !lhs.is_flipped_horizontally;

					if (&lhs != &rhs)
					{
						rhs.is_flipped_horizontally = !rhs.is_flipped_horizontally;
						std::swap(lhs, rhs);
					}
				}
			}
		}

		if (flip_y)
		{
			for (size_t x = 0; x < BrushWidth(); ++x)
			{
				for (size_t y = 0; y < ((BrushHeight() / 2) + (BrushHeight() % 2)); ++y)
				{
					const size_t y_offset = y * BrushWidth();
					const size_t y_offset_inverse = ((BrushHeight() - 1) - y) * BrushWidth();
					rom::TileInstance& lhs = out_tiles[y_offset + x];
					rom::TileInstance& rhs = out_tiles[y_offset_inverse + x];
					lhs.is_flipped_vertically = !lhs.is_flipped_vertically;

					if (&lhs != &rhs)
					{
						rhs.is_flipped_vertically = !rhs.is_flipped_vertically;
						std::swap(lhs, rhs);
					}
				}
			}
		}

		return out_tiles;
	}

	void TileBrush::CacheSymmetryFlags(const TileSet& tile_set)
	{
		is_x_symmetrical = IsBrushSymmetrical(tile_set, true, false);
		is_y_symmetrical = IsBrushSymmetrical(tile_set, false, true);
	}

	bool TileBrush::IsBrushSymmetrical(const TileSet& tile_set, bool flip_x, bool flip_y) const
	{
		bool _x_symmetrical = flip_x;
		bool _y_symmetrical = flip_y;

		if (flip_x == false && flip_y == false)
		{
			return false;
		}

		rom::TileBrush flipped_brush = *this;
		flipped_brush.tiles = std::move(TilesFlipped(flip_x, flip_y));
		return IsBrushSymmetricallyEqualTo(flipped_brush, tile_set, flip_x, flip_y);
	}

	bool TileBrush::IsBrushSymmetricallyEqualTo(const rom::TileBrush& flipped_brush, const TileSet& tile_set, bool flip_x, bool flip_y) const
	{
		bool _x_symmetrical = flip_x;
		bool _y_symmetrical = flip_y;

		if (flip_x == false && flip_y == false)
		{
			return this == &flipped_brush || *this == flipped_brush;
		}

		for (size_t i = 0; i < flipped_brush.tiles.size(); ++i)
		{
			if (tiles[i].tile_index != flipped_brush.tiles[i].tile_index
				|| tiles[i].palette_line != flipped_brush.tiles[i].palette_line
				|| tiles[i].is_high_priority != flipped_brush.tiles[i].is_high_priority)
			{
				return false;
			}

			if (tiles[i].tile_index >= tile_set.tiles.size())
			{
				continue;
			}

			if (flip_x && tile_set.tiles[tiles[i].tile_index].is_x_symmetrical == false
				&& tiles[i].is_flipped_horizontally != flipped_brush.tiles[i].is_flipped_horizontally)
			{
				_x_symmetrical = false;
			}

			if (flip_y && tile_set.tiles[tiles[i].tile_index].is_y_symmetrical == false
				&& tiles[i].is_flipped_vertically != flipped_brush.tiles[i].is_flipped_vertically)
			{
				_y_symmetrical = false;
			}
		}

		return (flip_x == false || _x_symmetrical) && (flip_y == false || _y_symmetrical);
	}

	size_t TileBrush::GridCoordinatesToLinearIndex(Point grid_coord) const
	{
		return grid_coord.x + (grid_coord.y * BrushWidth());

	}

	Point TileBrush::LinearIndexToGridCoordinates(size_t linear_index) const
	{
		Point out_coord{};
		out_coord.x = (linear_index % BrushWidth());
		out_coord.y = static_cast<int>(((linear_index - (linear_index % BrushWidth())) / static_cast<float>(BrushWidth())));

		return out_coord;
	}

	TileBrush::TileBrush(Uint32 width, Uint32 height)
		: m_brush_width(width)
		, m_brush_height(height)
		, m_brush_total_tiles(width * height)
	{

	}

	void TileBrush::AssignTiles(const std::vector<TileInstance>& source_tiles)
	{
		if (source_tiles.size() == tiles.size())
		{
			tiles = source_tiles;
		}
	}

	SDLSurfaceHandle TileBrush::RenderToSurface(const TileLayer& tile_layer) const
	{
		rom::Sprite brush_sprite = {};

		for (size_t i = 0; i < tiles.size(); ++i)
		{
			const rom::TileInstance& tile = tiles[i];
			std::shared_ptr<rom::SpriteTile> sprite_tile = tile_layer.tileset->CreateSpriteTileFromTile(tile.tile_index);

			if (sprite_tile == nullptr)
			{
				break;
			}

			const size_t current_brush_offset = i;

			sprite_tile->x_offset = static_cast<Sint16>(current_brush_offset % BrushWidth()) * rom::TileSet::s_tile_width;
			sprite_tile->y_offset = static_cast<Sint16>((current_brush_offset - (current_brush_offset % BrushWidth())) / BrushWidth()) * rom::TileSet::s_tile_height;

			sprite_tile->blit_settings.flip_horizontal = tile.is_flipped_horizontally;
			sprite_tile->blit_settings.flip_vertical = tile.is_flipped_vertically;

			sprite_tile->blit_settings.palette = tile_layer.palette_set.palette_lines.at(tile.palette_line);
			brush_sprite.sprite_tiles.emplace_back(std::move(sprite_tile));
		}
		brush_sprite.num_tiles = static_cast<Uint16>(brush_sprite.sprite_tiles.size());

		SDLSurfaceHandle new_surface{ SDL_CreateSurface(brush_sprite.GetBoundingBox().Width(), brush_sprite.GetBoundingBox().Height(), SDL_PIXELFORMAT_RGBA32) };
		SDL_SetSurfaceColorKey(new_surface.get(), true, SDL_MapRGBA(SDL_GetPixelFormatDetails(new_surface->format), nullptr, 0, 0, 0, 0));
		SDL_ClearSurface(new_surface.get(), 0.0f, 0, 0, 0);
		brush_sprite.RenderToSurface(new_surface.get());
		return std::move(new_surface);
	}

}