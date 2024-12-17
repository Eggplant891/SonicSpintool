#include "ui/ui_sprite.h"

namespace spintool
{
	UISpriteTexture::UISpriteTexture(std::shared_ptr<const rom::Sprite>& spr)
		: sprite(spr)
		, dimensions(static_cast<float>(sprite->GetBoundingBox().Width()), static_cast<float>(sprite->GetBoundingBox().Height()))
	{
		for (const std::shared_ptr<rom::SpriteTile>& sprite_tile : sprite->sprite_tiles)
		{
			tile_textures.emplace_back(sprite_tile);
		}
	}

	void UISpriteTexture::DrawForImGui(const float zoom /*= 1.0f*/) const
	{
		ImGui::Image((ImTextureID)texture.get()
			, ImVec2(static_cast<float>(dimensions.x) * zoom, static_cast<float>(dimensions.y) * zoom)
			, { 0,0 }
		, { static_cast<float>(dimensions.x) / texture->w, static_cast<float>((dimensions.y) / texture->h) });
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

	UISpriteTileTexture::UISpriteTileTexture(const std::shared_ptr<rom::SpriteTile>& spr)
		: sprite_tile(spr)
		, dimensions(static_cast<float>(spr->x_size), static_cast<float>(spr->y_size))
	{

	}

	void UISpriteTileTexture::DrawForImGui(const float zoom/*= 1.0f*/) const
	{
		ImGui::Image((ImTextureID)texture.get()
			, ImVec2(static_cast<float>(dimensions.x) * zoom, static_cast<float>(dimensions.y) * zoom)
			, { 0,0 }
		, { static_cast<float>(dimensions.x) / texture->w, static_cast<float>((dimensions.y) / texture->h) });
	}

	SDLTextureHandle UISpriteTileTexture::RenderTextureForPalette(const UIPalette& palette) const
	{
		Renderer::s_sdl_update_mutex.lock();

		Renderer::SetPalette(palette.sdl_palette);
		SDLTextureHandle new_tex = Renderer::RenderToTexture(*sprite_tile);

		Renderer::s_sdl_update_mutex.unlock();

		return std::move(new_tex);
	}
}