#pragma once

#include <optional>
#include <filesystem>
#include <vector>

namespace spintool
{
	class EditorUI;
}

namespace spintool
{
	struct FileSelectorSettings
	{
		std::string object_typename;
		std::filesystem::path target_directory;
		std::vector<std::string> file_extension_filter;
		int num_columns = 1;
		bool open_popup = false;
		bool close_popup = false;
		bool tiled_previews = false;
	};

	std::optional<std::filesystem::path> DrawFileSelector(const FileSelectorSettings& settings, EditorUI& owning_ui, std::optional<std::filesystem::path> current_selection);
}
