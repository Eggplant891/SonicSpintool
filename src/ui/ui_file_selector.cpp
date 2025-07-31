#include "ui/ui_file_selector.h"

#include "imgui.h"
#include "ui/ui_editor.h"

namespace spintool
{
	std::optional<std::filesystem::path> spintool::DrawFileSelector(const FileSelectorSettings& settings, EditorUI& owning_ui, std::optional<std::filesystem::path> current_selection)
	{
		static std::optional<std::filesystem::path> s_highlighted_path;
		std::optional<std::filesystem::path> return_path;
		const std::string popup_title = "Choose " + settings.object_typename;

		if (settings.open_popup && settings.close_popup == false)
		{
			ImGui::OpenPopup(popup_title.c_str(), ImGuiPopupFlags_AnyPopupLevel);
			s_highlighted_path = current_selection;
		}

		if (ImGui::IsPopupOpen(popup_title.c_str()))
		{
			ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		}

		if (ImGui::BeginPopup(popup_title.c_str(), ImGuiWindowFlags_Modal))
		{
			if (settings.close_popup)
			{
				s_highlighted_path.reset();
				ImGui::CloseCurrentPopup();
			}

			ImGui::Text(settings.target_directory.generic_u8string().c_str());
			ImGui::Separator();
			bool has_files = false;
			for (const std::filesystem::path& filepath : std::filesystem::directory_iterator{ settings.target_directory })
			{
				if (std::any_of(std::begin(settings.file_extension_filter), std::end(settings.file_extension_filter),
					[filepath](const std::string_view ext)
					{
						return filepath.extension() == ext;
					}))
				{
					bool is_selected = s_highlighted_path == filepath;
					has_files = true;
					if (ImGui::Selectable(filepath.filename().generic_u8string().c_str(), &is_selected, ImGuiSelectableFlags_NoAutoClosePopups))
					{
						s_highlighted_path = filepath;
					}
					if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
					{
						return_path = s_highlighted_path;
					}
				}
			}

			ImGui::BeginDisabled(s_highlighted_path.has_value() == false);
			if (has_files == false)
			{
				ImGui::Text("<No valid %s detected>", settings.object_typename.c_str());
				const std::filesystem::path path_diff = std::filesystem::relative(settings.target_directory, std::filesystem::current_path());
				ImGui::Text("<Add to the %s folder in the executable directory>", path_diff.generic_u8string().c_str());
			}
			const std::string selection_str = "Choose " + settings.object_typename;
			if (ImGui::Button(selection_str.c_str()) && s_highlighted_path.has_value())
			{
				return_path = s_highlighted_path;
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
			{
				s_highlighted_path.reset();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndDisabled();
			ImGui::EndPopup();
		}

		return return_path;
	}
}
