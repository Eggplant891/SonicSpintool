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

		std::string m_project_name;

		std::filesystem::path m_rom_location;
		std::filesystem::path m_project_path_root;

		std::vector<editor::Level> m_levels;
	};
}
