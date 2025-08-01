#include "project/editor_project.h"

#include "ui/ui_editor.h"

#include "nlohmann/json.hpp"

#include <fstream>

namespace spintool
{

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

		return loaded_project;
	}

}