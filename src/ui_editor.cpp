#include "ui_editor.h"

#include "spinball_rom.h"
#include "render.h"

#include "imgui.h"
#include <thread>
#include <algorithm>

namespace spintool
{
	EditorUI::EditorUI()
		: m_sprite_navigator(*this)
	{

	}

	void EditorUI::Initialise()
	{
		m_rom.LoadROM();
		std::vector<VDPPalette> out_palettes = m_rom.LoadPalettes(32);
		m_sprite_navigator.SetPalettes(out_palettes);
	}

	void EditorUI::Update()
	{
		m_sprite_navigator.Update();

		for (EditorSpriteViewer& sprite_window : m_sprite_viewer_windows)
		{
			sprite_window.Update();
		}

		auto new_end_it = std::remove_if(std::begin(m_sprite_viewer_windows), std::end(m_sprite_viewer_windows),
			[](const EditorSpriteViewer& sprite_window)
			{
				return sprite_window.IsOpen() == false;
			});

		m_sprite_viewer_windows.erase(new_end_it, std::end(m_sprite_viewer_windows));
	}

	void EditorUI::Shutdown()
	{

	}

	SpinballROM& EditorUI::GetROM()
	{
		return m_rom;
	}

	void EditorUI::OpenSpriteViewer(const SpinballSprite& sprite, const std::vector<UIPalette>& palettes)
	{
		auto selected_sprite_window_it = std::find_if(std::begin(m_sprite_viewer_windows), std::end(m_sprite_viewer_windows),
			[&sprite](const EditorSpriteViewer& sprite_viewer)
			{
				return sprite_viewer.m_sprites->sprite.rom_offset == sprite.rom_offset;
			});
		if (selected_sprite_window_it == std::end(m_sprite_viewer_windows))
		{
			m_sprite_viewer_windows.emplace_back(sprite, palettes);
		}
	}

}