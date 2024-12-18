#include "ui/ui_editor.h"

#include "rom/spinball_rom.h"
#include "render.h"
#include "SDL3/SDL_image.h"
#include "imgui.h"
#include <thread>
#include <algorithm>
#include "ui/ui_file_selector.h"

namespace spintool
{
	EditorUI::EditorUI()
		: m_sprite_navigator(*this)
		, m_tileset_navigator(*this)
		, m_palette_viewer(*this)
		, m_sprite_importer(*this)
	{
		m_rom_load_path = std::filesystem::current_path().append("roms");
		if (std::filesystem::exists(m_rom_load_path) == false)
		{
			std::filesystem::create_directory(m_rom_load_path);
		}
		m_sprite_export_path = std::filesystem::current_path().append("sprite_export");
		if (std::filesystem::exists(m_sprite_export_path) == false)
		{
			std::filesystem::create_directory(m_sprite_export_path);
		}
		m_rom_export_path = std::filesystem::current_path().append("rom_export");
		if (std::filesystem::exists(m_rom_export_path) == false)
		{
			std::filesystem::create_directory(m_rom_export_path);
		}
	}

	SDLTextureHandle tails;

	bool EditorUI::AttemptLoadROM(const std::filesystem::path& rom_path)
	{
		if (m_rom.LoadROMFromPath(rom_path))
		{
			m_rom_path = rom_path;
			m_palettes = m_rom.LoadPalettes(48);
			return true;
		}

		return false;
	}

	void EditorUI::Initialise()
	{
		AttemptLoadROM(m_rom_path);
	}

	void EditorUI::Update()
	{
		float menu_bar_height = 0;
		bool open_rom_popup = false;

		if (ImGui::BeginMainMenuBar())
		{
			if (IsROMLoaded())
			{
				if (ImGui::BeginMenu("File"))
				{
					if (ImGui::MenuItem("Save ROM"))
					{
						m_rom.SaveROM();
					}

					if (ImGui::MenuItem("Reload ROM"))
					{
						AttemptLoadROM(m_rom_path);
					}
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("Tools"))
				{
					ImGui::MenuItem("Sprite Navigator", nullptr, &m_sprite_navigator.visible);
					ImGui::MenuItem("Tileset Navigator", nullptr, &m_tileset_navigator.visible);
					ImGui::MenuItem("Palettes", nullptr, &m_palette_viewer.visible);
					ImGui::Separator();
					ImGui::MenuItem("Sprite Importer", nullptr, &m_sprite_importer.visible);
					ImGui::EndMenu();
				}
				ImGui::SameLine();
			}
			ImGui::BeginDisabled();
			ImGui::Text(m_rom_path.filename().generic_u8string().c_str());
			ImGui::EndDisabled();
			ImGui::SameLine();
			if (ImGui::Button("Change ROM Filename"))
			{
				open_rom_popup = true;
			}

			menu_bar_height = ImGui::GetWindowHeight();
			ImGui::EndMainMenuBar();
		}

		static FileSelectorSettings settings;
		settings.open_popup = open_rom_popup;
		settings.object_typename = "ROM";
		settings.target_directory = GetROMLoadPath();
		settings.file_extension_filter = { ".bin", ".md" };

		std::optional<std::filesystem::path> selected_path = DrawFileSelector(settings, *this, m_rom_path);

		settings.close_popup = false;
		if (selected_path && AttemptLoadROM(selected_path.value()))
		{
			settings.close_popup = true;
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
		m_tileset_navigator.Update();
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

	bool EditorUI::IsROMLoaded() const
	{
		return m_rom.m_buffer.empty() == false;
	}

	rom::SpinballROM& EditorUI::GetROM()
	{
		return m_rom;
	}

	std::filesystem::path EditorUI::GetROMLoadPath() const
	{
		return m_rom_load_path;
	}
	
	std::filesystem::path EditorUI::GetROMExportPath() const
	{
		return m_rom_export_path;
	}

	std::filesystem::path EditorUI::GetSpriteExportPath() const
	{
		return m_sprite_export_path;
	}

	const std::vector<std::shared_ptr<const rom::TileSet>>& EditorUI::GetTilesets() const
	{
		return m_tileset_navigator.m_tilesets;
	}

	const std::vector<rom::Palette>& EditorUI::GetPalettes() const
	{
		return m_palettes;
	}

	void EditorUI::OpenSpriteViewer(std::shared_ptr<const rom::Sprite>& sprite)
	{
		const auto selected_sprite_window_it = std::find_if(std::begin(m_sprite_viewer_windows), std::end(m_sprite_viewer_windows),
			[&sprite](const std::unique_ptr<EditorSpriteViewer>& sprite_viewer)
			{
				return sprite_viewer->GetOffset() == sprite->rom_data.rom_offset;
			});

		if (selected_sprite_window_it == std::end(m_sprite_viewer_windows))
		{
			m_sprite_viewer_windows.emplace_back(std::make_unique<EditorSpriteViewer>(*this, sprite));
		}
	}

	void EditorUI::OpenSpriteImporter(int rom_offset)
	{
		m_sprite_importer.visible = true;
		m_sprite_importer.ChangeTargetWriteLocation(rom_offset);
	}

}