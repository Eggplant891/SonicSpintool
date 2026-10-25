#pragma once

#include "editor/editor_level.h"

#include <string>
#include <vector>
#include <filesystem>
#include <memory>

namespace spintool
{
	namespace rom
	{
		class SpinballROM;
	}
}

namespace spintool
{
	class Project
	{
	public:
		void SaveProject();
		static std::unique_ptr<Project> CreateProject(const std::string& project_name, const rom::SpinballROM& src_rom);
		static std::unique_ptr<Project> LoadProject(const std::filesystem::path& project_path);

		[[nodiscard]] std::filesystem::path GetLevelsPath() const;

		// Moves the project folder to sit alongside the current one under new_name, then saves.
		[[nodiscard]] bool Rename(const std::string& new_name, std::string& out_error);
		// Saves a copy of the project under new_name and makes it the current project. The original is left on disk.
		[[nodiscard]] bool SaveAs(const std::string& new_name, std::string& out_error);
		[[nodiscard]] static bool IsValidProjectName(const std::string& project_name, std::string& out_error);

		std::string m_project_name;

		std::filesystem::path m_rom_location;
		std::filesystem::path m_project_path_root;

		std::vector<editor::Level> m_levels;
	};
}
