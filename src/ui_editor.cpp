#include "ui_editor.h"

#include "spinball_rom.h"
#include "render.h"
#include "SDL3/SDL_image.h"
#include "imgui.h"
#include <thread>
#include <algorithm>

namespace spintool
{
	EditorUI::EditorUI()
		: m_sprite_navigator(*this)
		, m_palette_viewer(*this)
		, m_sprite_importer(*this)
	{

	}

	SDLTextureHandle tails;

	void EditorUI::Initialise()
	{
		m_rom.LoadROMFromPath(m_rom_path);
		m_palettes = m_rom.LoadPalettes(48);
	}

	void EditorUI::Update()
	{
		float menu_bar_height = 0;
		bool open_rom_popup = false;

		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("Tools"))
			{
				ImGui::MenuItem("Sprite Navigator", nullptr, &m_sprite_navigator.visible);
				ImGui::MenuItem("Sprite Importer", nullptr, &m_sprite_importer.visible);
				ImGui::MenuItem("Palettes", nullptr, &m_palette_viewer.visible);
				if (ImGui::MenuItem("Save ROM"))
				{
					m_rom.SaveROM();
				}
				ImGui::EndMenu();
			}
			ImGui::SameLine();
			ImGui::BeginDisabled();
			ImGui::Text(m_rom_path);
			ImGui::EndDisabled();
			ImGui::SameLine();
			if (ImGui::Button("Change ROM Path"))
			{
				open_rom_popup = true;
			}

			menu_bar_height = ImGui::GetWindowHeight();
			ImGui::EndMainMenuBar();
		}
		static char temp_path[4096];

		if (open_rom_popup)
		{
			ImGui::OpenPopup("Edit ROM Path", ImGuiPopupFlags_AnyPopupLevel);
			sprintf_s(temp_path, "%s", m_rom_path);
		}

		if (ImGui::BeginPopup("Edit ROM Path"))
		{
			ImGui::SetNextItemWidth(1024);
			ImGui::InputText("Path", temp_path, sizeof(temp_path));
			if (ImGui::Button("Load ROM"))
			{
				sprintf_s(m_rom_path, "%s", temp_path);
				m_rom.LoadROMFromPath(m_rom_path);
				m_palettes = m_rom.LoadPalettes(48);
			}
			ImGui::EndPopup();
		}

		if (SDL_Texture* viewport = Renderer::GetViewportTexture())
		{
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0,0 });
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0,0 });

			ImVec2 dim{ static_cast<float>(Renderer::window_width), static_cast<float>(Renderer::window_height) - menu_bar_height };

			ImGui::SetNextWindowSize(dim, ImGuiCond_Always);
			ImGui::SetNextWindowPos({ 0, menu_bar_height }, ImGuiCond_Always);

			if (ImGui::Begin("main_viewport", nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoNavInputs))
			{
				float adjusted_height_uv = dim.y / Renderer::window_height;
				if (viewport)
				{
					ImGui::Image((ImTextureID)viewport, dim, { 0,0 }, { 1, adjusted_height_uv });
				}
				else
				{
					ImGui::Dummy(dim);
				}
			}
			ImGui::End();
			ImGui::PopStyleVar(2);
		}
		
		m_sprite_importer.Update();
		m_sprite_navigator.Update();
		m_palette_viewer.Update(m_palettes);

		for (std::unique_ptr<EditorSpriteViewer>& sprite_window : m_sprite_viewer_windows)
		{
			sprite_window->Update();
		}

		auto new_end_it = std::remove_if(std::begin(m_sprite_viewer_windows), std::end(m_sprite_viewer_windows),
			[](const std::unique_ptr<EditorSpriteViewer>& sprite_window)
			{
				return sprite_window->IsOpen() == false;
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

	const std::vector<VDPPalette>& EditorUI::GetPalettes() const
	{
		return m_palettes;
	}

	void EditorUI::OpenSpriteViewer(std::shared_ptr<const SpinballSprite>& sprite)
	{
		const auto selected_sprite_window_it = std::find_if(std::begin(m_sprite_viewer_windows), std::end(m_sprite_viewer_windows),
			[&sprite](const std::unique_ptr<EditorSpriteViewer>& sprite_viewer)
			{
				return sprite_viewer->GetOffset() == sprite->rom_offset;
			});

		if (selected_sprite_window_it == std::end(m_sprite_viewer_windows))
		{
			m_sprite_viewer_windows.emplace_back(std::make_unique<EditorSpriteViewer>(*this, sprite));
		}
	}

}