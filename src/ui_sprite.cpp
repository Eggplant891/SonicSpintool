#include "ui_sprite.h"

namespace spintool
{
	UISpriteTexture::UISpriteTexture(const std::shared_ptr<SpinballSprite>& spr)
		: sprite(spr)
		, dimensions(static_cast<float>(sprite->GetBoundingBox().Width()), static_cast<float>(sprite->GetBoundingBox().Height()))
	{
		for (const std::shared_ptr<SpriteTile>& sprite_tile : sprite->sprite_tiles)
		{
			tile_textures.emplace_back(sprite_tile);
		}
	}

	SDLTextureHandle UISpriteTexture::RenderTextureForPalette(const UIPalette& palette) const
	{
		Renderer::s_sdl_update_mutex.lock();

		Renderer::SetPalette(palette.sdl_palette);
		SDLTextureHandle new_tex = Renderer::RenderToTexture(*sprite);

		for (const UISpriteTileTexture& tile_texture : tile_textures)
		{
			tile_texture.texture = tile_texture.RenderTextureForPalette(palette);
		}

		Renderer::s_sdl_update_mutex.unlock();

		return std::move(new_tex);
	}

	UISpriteTileTexture::UISpriteTileTexture(const std::shared_ptr<SpriteTile>& spr)
		: sprite_tile(spr)
		, dimensions(static_cast<float>(spr->x_size), static_cast<float>(spr->y_size))
	{

	}
	SDLTextureHandle UISpriteTileTexture::RenderTextureForPalette(const UIPalette& palette) const
	{
		Renderer::s_sdl_update_mutex.lock();

		Renderer::SetPalette(palette.sdl_palette);
		SDLTextureHandle new_tex = Renderer::RenderToTexture(*sprite_tile);

		Renderer::s_sdl_update_mutex.unlock();

		return std::move(new_tex);
	}

	UISpriteTextureAllPalettes::UISpriteTextureAllPalettes(const std::shared_ptr<SpinballSprite>& spr, const std::vector<UIPalette>& palettes)
		: sprite(spr)
	{
		Renderer::s_sdl_update_mutex.lock();
		for (const UIPalette& palette : palettes)
		{
			UISpriteTexture& texture = textures.emplace_back(sprite);
			texture.texture = texture.RenderTextureForPalette(palette);
		}

		Renderer::s_sdl_update_mutex.unlock();
	}
}