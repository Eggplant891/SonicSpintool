#include "ui_sprite.h"

namespace spintool
{
	UISpriteTexture::UISpriteTexture(const SpinballSprite& sprite)
		: sprite(sprite)
		, dimensions(static_cast<float>(sprite.x_size), static_cast<float>(sprite.y_size))
	{

	}

	SDLTextureHandle UISpriteTexture::RenderTextureForPalette(const UIPalette& palette) const
	{
		Renderer::s_sdl_update_mutex.lock();

		Renderer::SetPalette(palette.sdl_palette);
		SDLTextureHandle new_tex = Renderer::RenderToTexture(sprite);

		Renderer::s_sdl_update_mutex.unlock();

		return std::move(new_tex);
	}

	UISpriteTextureAllPalettes::UISpriteTextureAllPalettes(const SpinballSprite& spr, const std::vector<UIPalette>& palettes)
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