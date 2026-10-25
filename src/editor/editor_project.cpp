#include "editor/editor_project.h"

#include "ui/ui_editor.h"
#include "serialisation/editor_serialiser.h"
#include "nlohmann/json.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace spintool
{
	namespace
	{
		constexpr int level_count = 4;
	}

	std::filesystem::path Project::GetLevelsPath() const
	{
		std::filesystem::path levels_path = m_project_path_root;
		levels_path.append("levels");
		return levels_path;
	}

	void Project::SaveProject()
	{
		std::filesystem::path project_path = m_project_path_root;
		if (std::filesystem::exists(project_path) == false)
		{
			std::filesystem::create_directory(project_path);
		}

		project_path.append("project.json");

		std::ofstream project_config_out{ project_path };
		nlohmann::json project_config_json_writer;
		project_config_json_writer["name"] = m_project_name;
		project_config_json_writer["rom_location"] = m_rom_location;

		nlohmann::json level_filenames = nlohmann::json::array();
		for (const editor::Level& level : m_levels)
		{
			try
			{
				std::unique_ptr<Serialiser> level_serialiser = Serialiser::OpenFile(GetLevelsPath(), level.level_filename);
				level_serialiser->Writer() = level.SerialiseToJSON();
				level_serialiser->Finalise();
				level_filenames.emplace_back(level.level_filename);
			}
			catch (const std::runtime_error& error)
			{
				std::cerr << "Failed to save level " << level.m_level_index << ": " << error.what() << '\n';
			}
		}
		project_config_json_writer["levels"] = level_filenames;

		project_config_out << project_config_json_writer.dump(4);
	}

	std::unique_ptr<spintool::Project> Project::CreateProject(const std::string& project_name, const rom::SpinballROM& src_rom)
	{
		std::unique_ptr<Project> new_project = std::make_unique<Project>();
		std::filesystem::path config_path = EditorUI::GetProjectsPath();
		config_path.append(project_name);

		new_project->m_project_name = project_name;
		new_project->m_project_path_root = config_path;
		new_project->m_rom_location = src_rom.m_filepath;

		new_project->m_levels.reserve(level_count);
		for (int i = 0; i < level_count; ++i)
		{
			editor::Level& new_level = new_project->m_levels.emplace_back(editor::Level::LoadFromROM(src_rom, i));
			new_level.level_filename = "level_" + std::to_string(i) + ".json";
		}

		new_project->SaveProject();

		return new_project;
	}

	std::unique_ptr<Project> Project::LoadProject(const std::filesystem::path& project_path)
	{
		std::filesystem::path working_path = project_path;
		if (working_path.has_filename() == false || working_path.filename() != "project.json")
		{
			working_path.append("project.json");
		}

		std::unique_ptr<Project> loaded_project = std::make_unique<Project>();

		std::ifstream project_config_in{ working_path };
		nlohmann::json project_config_json_writer = nlohmann::json::parse(project_config_in);
		loaded_project->m_project_name = project_config_json_writer["name"];
		loaded_project->m_rom_location = std::filesystem::path{ std::string{ project_config_json_writer["rom_location"] } };
		loaded_project->m_project_path_root = working_path.parent_path();

		if (const auto levels_it = project_config_json_writer.find("levels");
			levels_it != project_config_json_writer.end() && levels_it->is_array())
		{
			for (const nlohmann::json& level_filename : *levels_it)
			{
				const std::string filename = level_filename.get<std::string>();
				try
				{
					std::unique_ptr<Deserialiser> level_deserialiser = Deserialiser::OpenFile(loaded_project->GetLevelsPath(), filename);
					if (level_deserialiser == nullptr)
					{
						std::cerr << "Missing level file: " << filename << '\n';
						continue;
					}

					editor::Level& loaded_level = loaded_project->m_levels.emplace_back(editor::Level::DeserialiseFromJSON(level_deserialiser->Reader()));
					loaded_level.level_filename = filename;
				}
				catch (const std::exception& error)
				{
					std::cerr << "Failed to load level " << filename << ": " << error.what() << '\n';
				}
			}
		}

		return loaded_project;
	}

}
