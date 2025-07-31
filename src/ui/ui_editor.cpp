#include "ui/ui_editor.h"

#include "render.h"
#include "rom/spinball_rom.h"
#include "ui/ui_file_selector.h"

#include "imgui.h"
#include "SDL3/SDL_image.h"
#include "nlohmann/json.hpp"

#include <thread>
#include <algorithm>
#include <numeric>
#include <fstream>


namespace spintool
{
	EditorUI::EditorUI()
		: m_sprite_navigator(*this)
		, m_tileset_navigator(*this)
		, m_tile_layout_viewer(*this)
		, m_palette_viewer(*this)
		, m_sprite_importer(*this)
		, m_animation_navigator(*this)
	{
		auto setup_directory = [](const std::string& dir_name)
			{
				std::filesystem::path out_path{ std::filesystem::current_path().append(dir_name) };
				if (std::filesystem::exists(out_path) == false)
				{
					std::filesystem::create_directory(out_path);
				}

				return out_path;
			};

		m_rom_load_path = setup_directory("roms");
		m_sprite_export_path = setup_directory("sprite_export");
		m_rom_export_path = setup_directory("rom_export");
		m_projects_path = setup_directory("projects");
		m_config_path = setup_directory("config");

		LoadROMConfig();
	}

	void EditorUI::SaveROMConfig() const
	{
		std::filesystem::path config_path = m_config_path;
		config_path.append("roms.json");

		std::ofstream config_out{ config_path };
		nlohmann::json config_json_writer;
		config_json_writer["usa_rom_path"] = m_usa_rom_path.generic_u8string();
		config_json_writer["eur_rom_path"] = "";
		config_json_writer["jp_rom_path"] = "";

		config_out << config_json_writer.dump(4);
	}

	void EditorUI::LoadROMConfig()
	{
		std::filesystem::path config_path = m_config_path;
		config_path.append("roms.json");

		if (std::filesystem::exists(config_path) == false)
		{
			SaveROMConfig();
		}

		std::ifstream config_in{ config_path };
		nlohmann::json config_json_reader{ nlohmann::json::parse(config_in) };
		std::string rom_path = config_json_reader["usa_rom_path"];
		if (rom_path.empty() == false)
		{
			AttemptLoadROM(rom_path);
		}
	}

	bool EditorUI::AttemptLoadROM(const std::filesystem::path& rom_path)
	{
		if (m_rom.LoadROMFromPath(rom_path))
		{
			m_usa_rom_path = rom_path;
			m_palettes = m_rom.LoadPalettes(48);
			SaveROMConfig();
			//m_palettes = m_rom.LoadPalettes(8);
			return true;
		}

		return false;
	}

	void EditorUI::Initialise()
	{
		AttemptLoadROM(m_usa_rom_path);
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
						AttemptLoadROM(m_usa_rom_path);
					}
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("Tools"))
				{
					ImGui::MenuItem("Sprite Navigator", nullptr, &m_sprite_navigator.m_visible);
					ImGui::MenuItem("Animation Navigator", nullptr, &m_animation_navigator.m_visible);
					ImGui::MenuItem("Tileset Navigator", nullptr, &m_tileset_navigator.m_visible);
					ImGui::MenuItem("Tile Layout Viewer", nullptr, &m_tile_layout_viewer.m_visible);
					ImGui::MenuItem("Palettes", nullptr, &m_palette_viewer.m_visible);
					ImGui::Separator();
					ImGui::MenuItem("Sprite Importer", nullptr, &m_sprite_importer.m_visible);
					ImGui::EndMenu();
				}
				ImGui::SameLine();
			}
			ImGui::BeginDisabled();
			ImGui::Text(m_usa_rom_path.filename().generic_u8string().c_str());
			ImGui::EndDisabled();
			ImGui::SameLine();
			if (ImGui::Button("Change ROM Filename"))
			{
				open_rom_popup = true;
			}

			static std::array<double, 32> rolling_frame_times;
			static size_t current_frame = 0;

			static std::chrono::steady_clock fps_clock;
			static std::chrono::time_point previous_poll_time = fps_clock.now();
			const std::chrono::time_point current_poll_time = fps_clock.now();
			const std::chrono::duration frame_time = current_poll_time - previous_poll_time;

			rolling_frame_times[current_frame] = static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(frame_time).count());
			
			const double rolling_average = std::accumulate(std::begin(rolling_frame_times), std::end(rolling_frame_times), 0.0,
				[](double frame_time, double rolling_average_in)
				{
					return frame_time + rolling_average_in;
				}) / static_cast<double>(std::size(rolling_frame_times));
			current_frame = (current_frame + 1) % std::size(rolling_frame_times);

			previous_poll_time = current_poll_time;

			const float content_region_remaining = ImGui::GetContentRegionAvail().x;
			const float offset = content_region_remaining - ImGui::CalcTextSize("FPS 9999").x;
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
			ImGui::Text("FPS %.0f", static_cast<double>(std::chrono::nanoseconds(std::chrono::seconds(1)).count()) / static_cast<double>(rolling_average));

			menu_bar_height = ImGui::GetWindowHeight();
			ImGui::EndMainMenuBar();
		}

		static FileSelectorSettings settings;
		settings.open_popup = open_rom_popup;
		settings.object_typename = "ROM";
		settings.target_directory = GetROMLoadPath();
		settings.file_extension_filter = { ".bin", ".md" };

		std::optional<std::filesystem::path> selected_path = DrawFileSelector(settings, *this, m_usa_rom_path);

		settings.close_popup = false;
		if (selected_path && AttemptLoadROM(selected_path.value()))
		{
			settings.close_popup = true;
		}

		if (SDL_Texture* viewport = Renderer::GetViewportTexture())
		{
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0,0 });
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0,0 });

			ImVec2 dim{ static_cast<float>(Renderer::s_window_width), static_cast<float>(Renderer::s_window_height) - menu_bar_height };

			ImGui::SetNextWindowSize(dim, ImGuiCond_Always);
			ImGui::SetNextWindowPos({ 0, menu_bar_height }, ImGuiCond_Always);

			if (ImGui::Begin("main_viewport", nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoNavInputs))
			{
				float adjusted_height_uv = dim.y / Renderer::s_window_height;
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
		m_tile_layout_viewer.Update();
		m_animation_navigator.Update();
		m_palette_viewer.Update();

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

	const std::vector<TilesetEntry>& EditorUI::GetTilesets() const
	{
		return m_tileset_navigator.m_tilesets;
	}

	const std::vector<std::shared_ptr<rom::Palette>>& EditorUI::GetPalettes() const
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
			auto& new_viewer = m_sprite_viewer_windows.emplace_back(std::make_unique<EditorSpriteViewer>(*this, sprite));
			new_viewer->m_visible = true;
		}
	}

	void EditorUI::OpenSpriteImporter(int rom_offset)
	{
		m_sprite_importer.m_visible = true;
		m_sprite_importer.ChangeTargetWriteLocation(rom_offset);
	}

}