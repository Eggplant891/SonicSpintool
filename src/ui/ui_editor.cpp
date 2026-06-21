#include "ui/ui_editor.h"

#include "render.h"
#include "rom/spinball_rom.h"
#include "ui/ui_file_selector.h"
#include "editor/editor_project.h"

#include "imgui.h"
#include "nlohmann/json.hpp"

#include <thread>
#include <array>
#include <chrono>
#include <optional>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <iostream>
#include <system_error>

#include "serialisation/editor_serialiser.h"

namespace spintool
{
	namespace
	{
		std::filesystem::path SetupDirectory(const std::string& dir_name)
		{
			std::error_code error;
			std::filesystem::path base_path =
				std::filesystem::current_path(error);

			if (error)
			{
				std::cerr << "Could not determine current working directory: "
					<< error.message() << '\n';
				base_path = std::filesystem::path{"."};
			}

			// Preserve SpinTool's original behaviour: all runtime folders are
			// resolved from the directory used to launch the application.
			// When SpinTool is launched from build/, this gives build/roms and
			// build/rom_export, exactly like the original code.
			const std::filesystem::path out_path = base_path / dir_name;

			error.clear();
			std::filesystem::create_directories(out_path, error);
			if (error)
			{
				std::cerr << "Could not create directory "
					<< out_path << ": " << error.message() << '\n';
			}

			return out_path;
		}

		bool CanOpenReferenceROM(const std::filesystem::path& path)
		{
			std::ifstream stream(path, std::ios::binary);
			return stream.is_open();
		}

	}

	std::filesystem::path EditorUI::s_rom_load_path = SetupDirectory("roms");
	std::filesystem::path EditorUI::s_sprite_export_path = SetupDirectory("sprite_export");
	std::filesystem::path EditorUI::s_rom_export_path = SetupDirectory("rom_export");
	std::filesystem::path EditorUI::s_projects_path = SetupDirectory("projects");
	std::filesystem::path EditorUI::s_metadata_path = SetupDirectory("metadata");
	std::filesystem::path EditorUI::s_config_path = SetupDirectory("config");

	EditorUI::EditorUI()
		: m_sprite_navigator(*this)
		, m_tileset_navigator(*this)
		, m_tile_layout_viewer(*this)
		, m_animation_navigator(*this)
		, m_palette_viewer(*this)
		, m_sprite_importer(*this)
	{
		LoadROMConfig();
		LoadUIConfig();
	}

	void EditorUI::SaveROMConfig() const
	{
		std::unique_ptr<Serialiser> serialiser =
			Serialiser::OpenFile(s_config_path, "roms.json");
		nlohmann::json& config_json_writer = serialiser->Writer();

		for (const rom::ROMMetadata& metadata : m_metadata.rom_metadatas)
		{
			config_json_writer[metadata.version_id] =
				metadata.location_on_disk.string();
		}
	}

	void EditorUI::LoadROMConfig()
	{
		const std::filesystem::path config_path = s_config_path / "roms.json";

		if (!Deserialiser::FileExists(config_path))
		{
			SaveROMConfig();
		}

		std::unique_ptr<Deserialiser> deserialiser;
		try
		{
			deserialiser = Deserialiser::OpenFile(config_path);
		}
		catch (const std::exception& error)
		{
			std::cerr << "Could not parse " << config_path
				<< ": " << error.what() << '\n';
			std::error_code ec;
			std::filesystem::rename(
				config_path,
				config_path.string() + ".invalid",
				ec
			);
			SaveROMConfig();
			return;
		}

		if (!deserialiser)
		{
			std::cerr << "Could not open ROM configuration: "
				<< config_path << '\n';
			return;
		}

		const nlohmann::json& config_json_reader = deserialiser->Reader();
		if (!config_json_reader.is_object())
		{
			std::cerr << "ROM configuration is not a JSON object: "
				<< config_path << '\n';
			return;
		}

		for (auto& rom_metadata : m_metadata.rom_metadatas)
		{
			if (rom_metadata.version_id.empty())
			{
				continue;
			}

			const auto entry = config_json_reader.find(rom_metadata.version_id);
			if (entry == config_json_reader.end() || entry->is_null())
			{
				continue;
			}

			if (!entry->is_string())
			{
				std::cerr
					<< "Ignoring invalid ROM path for version "
					<< rom_metadata.version_id
					<< ": expected a string, got "
					<< entry->type_name()
					<< '\n';
				continue;
			}

			const std::string rom_path = entry->get<std::string>();
			if (rom_path.empty())
			{
				continue;
			}

			try
			{
				AttemptLoadROM(std::filesystem::path{rom_path});
			}
			catch (const std::exception& error)
			{
				std::cerr
					<< "Could not load configured ROM "
					<< rom_path
					<< ": "
					<< error.what()
					<< '\n';
			}
		}
	}

	void EditorUI::SaveUIConfig() const
	{
		try
		{
			std::unique_ptr<Serialiser> serialiser =
				Serialiser::OpenFile(s_config_path, "ui.json");
			nlohmann::json& writer = serialiser->Writer();
			writer["font_scale_percent"] =
				static_cast<int>(m_font_scale * 100.0f + 0.5f);
		}
		catch (const std::exception& error)
		{
			std::cerr << "Could not save UI configuration: "
				<< error.what() << '\n';
		}
	}

	void EditorUI::LoadUIConfig()
	{
		const std::filesystem::path config_path = s_config_path / "ui.json";
		m_font_scale = 1.0f;

		if (Deserialiser::FileExists(config_path))
		{
			try
			{
				std::unique_ptr<Deserialiser> deserialiser =
					Deserialiser::OpenFile(config_path);
				if (deserialiser)
				{
					const nlohmann::json& reader = deserialiser->Reader();
					auto entry = reader.find("font_scale_percent");
					if (entry != reader.end() && entry->is_number_integer())
					{
						const int percent =
							std::clamp(entry->get<int>(), 50, 250);
						m_font_scale = static_cast<float>(percent) / 100.0f;
					}
				}
			}
			catch (const std::exception& error)
			{
				std::cerr << "Could not load UI configuration: "
					<< error.what() << '\n';
			}
		}

		ImGui::GetIO().FontGlobalScale = m_font_scale;
	}

	bool EditorUI::AttemptLoadROM(const std::filesystem::path& rom_path)
	{
		if (rom_path.empty())
		{
			std::cerr << "Cannot load an empty ROM path\n";
			return false;
		}

		std::error_code filesystem_error;
		std::filesystem::create_directories(
			s_rom_load_path,
			filesystem_error
		);
		if (filesystem_error)
		{
			std::cerr << "Could not create ROM reference directory "
				<< s_rom_load_path << ": "
				<< filesystem_error.message() << '\n';
			return false;
		}

		filesystem_error.clear();
		std::filesystem::create_directories(
			s_rom_export_path,
			filesystem_error
		);
		if (filesystem_error)
		{
			std::cerr << "Could not create ROM export directory "
				<< s_rom_export_path << ": "
				<< filesystem_error.message() << '\n';
			return false;
		}

		// The immutable reference is always the file with the selected filename
		// inside the runtime roms/ directory. For a normal build-tree launch,
		// this is <project>/build/roms.
		const std::filesystem::path reference_path =
			s_rom_load_path / rom_path.filename();

		if (!CanOpenReferenceROM(reference_path))
		{
			std::cerr << "Reference ROM does not exist or cannot be read in roms/: "
				<< reference_path << '\n';
			m_reference_rom_path.clear();
			m_working_rom_path.clear();
			return false;
		}

		const std::filesystem::path working_path =
			s_rom_export_path / reference_path.filename();

		// Do not pre-test working_path with exists(): on some Linux/libstdc++
		// combinations a missing destination can leave ENOENT in error_code and
		// was incorrectly treated as a fatal inspection error. copy_file with
		// skip_existing handles both cases safely:
		//   - destination missing  -> copy is created;
		//   - destination present  -> existing working ROM is kept untouched.
		filesystem_error.clear();
		const bool working_rom_created = std::filesystem::copy_file(
			reference_path,
			working_path,
			std::filesystem::copy_options::skip_existing,
			filesystem_error
		);

		if (filesystem_error)
		{
			std::cerr << "Could not prepare working ROM "
				<< working_path << " from "
				<< reference_path << ": "
				<< filesystem_error.message() << '\n';
			return false;
		}

		if (working_rom_created)
		{
			std::cout << "Created working ROM: "
				<< working_path << '\n';
		}
		else
		{
			std::cout << "Using existing working ROM: "
				<< working_path << '\n';
		}

		if (!m_rom.LoadROMFromPath(working_path))
		{
			std::cerr << "Could not open working ROM: "
				<< working_path << '\n';
			return false;
		}

		m_reference_rom_path = reference_path;
		m_working_rom_path = working_path;

		m_current_rom_metadata = m_metadata.GetROMMetadataFor("usa");
		if (m_current_rom_metadata)
		{
			// Keep the immutable file in config. At the next launch,
			// AttemptLoadROM() reopens the matching rom_export copy.
			m_current_rom_metadata->location_on_disk = reference_path;
		}

		m_palettes = m_rom.LoadPalettes(48);

		std::cout << "Reference ROM: " << m_reference_rom_path << '\n';
		std::cout << "Working ROM:   " << m_working_rom_path << '\n';
		return true;
	}

	void EditorUI::Initialise()
	{
		m_current_rom_metadata = m_metadata.GetROMMetadataFor("usa");
		if (m_current_rom_metadata &&
			!m_current_rom_metadata->location_on_disk.empty())
		{
			AttemptLoadROM(m_current_rom_metadata->location_on_disk);
		}
	}

	void EditorUI::Update()
	{
		float menu_bar_height = 0;
		bool open_rom_popup = false;

		if (ImGui::BeginMainMenuBar())
		{
			if (IsROMLoaded())
			{
				/*if (ImGui::BeginMenu("File"))
				{
					if (ImGui::MenuItem("New Project..."))
					{
						Project::CreateProject("test_project", GetROM());
					}
					ImGui::Separator();

					if (ImGui::MenuItem("Export ROM"))
					{
						m_rom.SaveROM();
					}

					if (ImGui::MenuItem("Reload ROM"))
					{
						AttemptLoadROM(m_current_rom_metadata->location_on_disk);
					}

					if (ImGui::MenuItem("Export Metadata"))
					{
						auto serialiser =
							Serialiser::OpenFile(s_metadata_path, "metadata.json");
						auto& writer = serialiser->Writer();
						m_current_rom_metadata->Serialise(writer);
					}
					ImGui::EndMenu();
				}*/

				if (ImGui::BeginMenu("Tools"))
				{
					ImGui::MenuItem(
						"Sprite Navigator",
						nullptr,
						&m_sprite_navigator.m_visible
					);
					ImGui::MenuItem(
						"Animation Navigator",
						nullptr,
						&m_animation_navigator.m_visible
					);
					ImGui::MenuItem(
						"Tileset Navigator",
						nullptr,
						&m_tileset_navigator.m_visible
					);
					ImGui::MenuItem(
						"Tile Layout Viewer",
						nullptr,
						&m_tile_layout_viewer.m_visible
					);
					ImGui::MenuItem(
						"Palettes",
						nullptr,
						&m_palette_viewer.m_visible
					);
					/*ImGui::Separator();
					ImGui::MenuItem(
						"Sprite Importer",
						nullptr,
						&m_sprite_importer.m_visible
					);*/
					ImGui::EndMenu();
				}
				ImGui::SameLine();
			}

			if (ImGui::BeginMenu("Settings"))
			{
				ImGui::TextUnformatted("Font size");
				ImGui::SetNextItemWidth(180.0f);
				int font_percent =
					static_cast<int>(m_font_scale * 100.0f + 0.5f);
				if (ImGui::SliderInt(
					"##font_scale",
					&font_percent,
					50,
					250,
					"%d%%",
					ImGuiSliderFlags_AlwaysClamp
				))
				{
					m_font_scale =
						static_cast<float>(font_percent) / 100.0f;
					ImGui::GetIO().FontGlobalScale = m_font_scale;
				}
				if (ImGui::IsItemDeactivatedAfterEdit())
				{
					SaveUIConfig();
				}

				if (ImGui::MenuItem("Reset to 100%"))
				{
					m_font_scale = 1.0f;
					ImGui::GetIO().FontGlobalScale = m_font_scale;
					SaveUIConfig();
				}
				ImGui::EndMenu();
			}
			ImGui::SameLine();

			ImGui::BeginDisabled();
			if (m_current_rom_metadata &&
				!m_current_rom_metadata->location_on_disk.empty())
			{
				ImGui::Text(
					"%s",
					m_current_rom_metadata->location_on_disk
						.filename().string().c_str()
				);
			}
			else
			{
				ImGui::TextUnformatted("No ROM selected");
			}
			ImGui::EndDisabled();
			ImGui::SameLine();
			if (ImGui::Button("Change ROM Filename"))
			{
				open_rom_popup = true;
			}

			static std::array<double, 32> rolling_frame_times{};
			static size_t current_frame = 0;
			static std::chrono::time_point previous_poll_time =
				std::chrono::steady_clock::now();
			const std::chrono::time_point current_poll_time =
				std::chrono::steady_clock::now();
			const std::chrono::duration frame_time =
				current_poll_time - previous_poll_time;

			rolling_frame_times[current_frame] = static_cast<double>(
				std::chrono::duration_cast<std::chrono::nanoseconds>(frame_time)
					.count()
			);

			const double rolling_average = std::accumulate(
				std::begin(rolling_frame_times),
				std::end(rolling_frame_times),
				0.0,
				[](double frame_time_value, double rolling_average_in)
				{
					return frame_time_value + rolling_average_in;
				}
			) / static_cast<double>(std::size(rolling_frame_times));

			current_frame =
				(current_frame + 1) % std::size(rolling_frame_times);
			previous_poll_time = current_poll_time;

			const float content_region_remaining =
				ImGui::GetContentRegionAvail().x;
			const float offset =
				content_region_remaining - ImGui::CalcTextSize("FPS 9999").x;
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
			ImGui::Text(
				"FPS %.0f",
				static_cast<double>(
					std::chrono::nanoseconds(std::chrono::seconds(1)).count()
				) / rolling_average
			);

			menu_bar_height = ImGui::GetWindowHeight();
			ImGui::EndMainMenuBar();
		}

		static FileSelectorSettings settings;
		settings.open_popup = open_rom_popup;
		settings.object_typename = "ROM";
		settings.target_directory = GetROMLoadPath();
		settings.file_extension_filter = { ".bin", ".md" };

		const std::filesystem::path current_rom_path =
			m_current_rom_metadata
				? m_current_rom_metadata->location_on_disk
				: std::filesystem::path{};

		std::optional<std::filesystem::path> selected_path =
			DrawFileSelector(settings, *this, current_rom_path);

		settings.close_popup = false;
		if (selected_path && AttemptLoadROM(selected_path.value()))
		{
			SaveROMConfig();
			settings.close_popup = true;
		}

		{
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0, 0 });
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0, 0 });

			ImVec2 dim{
				static_cast<float>(Renderer::s_window_width),
				static_cast<float>(Renderer::s_window_height) - menu_bar_height
			};

			ImGui::SetNextWindowSize(dim, ImGuiCond_Always);
			ImGui::SetNextWindowPos({ 0, menu_bar_height }, ImGuiCond_Always);

			if (ImGui::Begin(
				"main_viewport",
				nullptr,
				ImGuiWindowFlags_NoSavedSettings |
					ImGuiWindowFlags_NoDecoration |
					ImGuiWindowFlags_NoBackground |
					ImGuiWindowFlags_NoInputs |
					ImGuiWindowFlags_NoFocusOnAppearing |
					ImGuiWindowFlags_NoBringToFrontOnFocus |
					ImGuiWindowFlags_NoNavFocus |
					ImGuiWindowFlags_NoNavInputs
			))
			{
				ImGui::Dummy(dim);
			}
			ImGui::End();
			ImGui::PopStyleVar(2);
		}

		// Do not catch exceptions around ImGui window rendering here.
		m_sprite_importer.Update();
		m_sprite_navigator.Update();
		m_tileset_navigator.Update();
		m_tile_layout_viewer.Update();
		m_animation_navigator.Update();
		m_palette_viewer.Update();

		for (std::unique_ptr<EditorSpriteViewer>& sprite_window :
			m_sprite_viewer_windows)
		{
			sprite_window->Update();
		}

		auto new_end_it = std::remove_if(
			std::begin(m_sprite_viewer_windows),
			std::end(m_sprite_viewer_windows),
			[](const std::unique_ptr<EditorSpriteViewer>& sprite_window)
			{
				return !sprite_window->IsOpen();
			}
		);

		m_sprite_viewer_windows.erase(
			new_end_it,
			std::end(m_sprite_viewer_windows)
		);
	}

	void EditorUI::Shutdown()
	{
		m_palette_viewer.Shutdown();
		m_animation_navigator.Shutdown();
		m_tile_layout_viewer.Shutdown();
		m_tileset_navigator.Shutdown();
		m_sprite_navigator.Shutdown();
		m_sprite_importer.Shutdown();
	}

	bool EditorUI::IsROMLoaded() const
	{
		return !m_rom.m_buffer.empty();
	}

	rom::SpinballROM& EditorUI::GetROM()
	{
		return m_rom;
	}

	std::filesystem::path EditorUI::GetROMLoadPath()
	{
		return s_rom_load_path;
	}

	std::filesystem::path EditorUI::GetROMExportPath()
	{
		return s_rom_export_path;
	}

	std::filesystem::path EditorUI::GetSpriteExportPath()
	{
		return s_sprite_export_path;
	}

	std::filesystem::path EditorUI::GetProjectsPath()
	{
		return s_projects_path;
	}

	std::filesystem::path EditorUI::GetMetadataPath()
	{
		return s_metadata_path;
	}

	std::filesystem::path EditorUI::GetConfigPath()
	{
		return s_config_path;
	}

	std::filesystem::path EditorUI::GetReferenceROMPath() const
	{
		return m_reference_rom_path;
	}

	std::filesystem::path EditorUI::GetWorkingROMPath() const
	{
		return m_working_rom_path;
	}

	const std::vector<TilesetEntry>& EditorUI::GetTilesets() const
	{
		return m_tileset_navigator.m_tilesets;
	}

	const std::vector<std::shared_ptr<rom::Palette>>&
	EditorUI::GetPalettes() const
	{
		return m_palettes;
	}

	void EditorUI::NotifyPaletteChanged()
	{
		m_sprite_navigator.InvalidatePaletteDependentTextures();
		for (const std::unique_ptr<EditorSpriteViewer>& viewer : m_sprite_viewer_windows)
		{
			if (viewer)
			{
				viewer->InvalidatePaletteDependentTextures();
			}
		}
	}

	void EditorUI::OpenSpriteViewer(
		std::shared_ptr<const rom::Sprite>& sprite
	)
	{
		const auto selected_sprite_window_it = std::find_if(
			std::begin(m_sprite_viewer_windows),
			std::end(m_sprite_viewer_windows),
			[&sprite](const std::unique_ptr<EditorSpriteViewer>& sprite_viewer)
			{
				return sprite_viewer->GetOffset() ==
					sprite->rom_data.rom_offset;
			}
		);

		if (selected_sprite_window_it == std::end(m_sprite_viewer_windows))
		{
			auto& new_viewer = m_sprite_viewer_windows.emplace_back(
				std::make_unique<EditorSpriteViewer>(*this, sprite)
			);
			new_viewer->m_visible = true;
		}
	}

	void EditorUI::OpenImageImporter(rom::Sprite& sprite)
	{
		m_sprite_importer.m_visible = true;
		m_sprite_importer.SetTarget(sprite);
		m_sprite_importer.SetAvailablePalettes(m_palettes);
	}

	void EditorUI::OpenImageImporter(
		rom::TileSet& tileset,
		const rom::PaletteSet& available_palettes
	)
	{
		m_sprite_importer.m_visible = true;
		m_sprite_importer.SetTarget(tileset);
		m_sprite_importer.SetAvailablePalettes(
			available_palettes.palette_lines
		);
	}
}
