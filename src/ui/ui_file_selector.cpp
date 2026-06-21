#include "ui/ui_file_selector.h"

#include "ui/ui_editor.h"

#include "imgui.h"

#include "sdl_handle_defs.h"

#include "SDL3/SDL_image.h"

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace spintool
{

namespace
{

std::string PathToUtf8(const std::filesystem::path& path)
{
#if defined(__cpp_lib_char8_t)
    const std::u8string utf8_path = path.generic_u8string();

    return std::string(
        reinterpret_cast<const char*>(utf8_path.data()),
        utf8_path.size()
    );
#else
    return path.generic_u8string();
#endif
}

struct FileSelectorEntry
{
    std::filesystem::path filepath{};
    SDLTextureHandle thumbnail{};
};

}

std::optional<std::filesystem::path> DrawFileSelector(
    const FileSelectorSettings& settings,
    EditorUI& owning_ui,
    std::optional<std::filesystem::path> current_selection
)
{
    static std::vector<FileSelectorEntry> s_file_entries;
    static std::optional<std::filesystem::path> s_highlighted_path;

    std::optional<std::filesystem::path> return_path;
    const std::string popup_title = "Choose " + settings.object_typename;

    if (settings.open_popup && settings.close_popup == false)
    {
        ImGui::OpenPopup(
            popup_title.c_str(),
            ImGuiPopupFlags_AnyPopupLevel
        );

        s_highlighted_path = current_selection;

        std::filesystem::directory_iterator directory_file_it{
            settings.target_directory
        };

        s_file_entries.clear();

        std::transform(
            std::filesystem::begin(directory_file_it),
            std::filesystem::end(directory_file_it),
            std::back_inserter(s_file_entries),
            [&settings](const std::filesystem::path& filepath)
            {
                const bool matches_extension = std::any_of(
                    std::begin(settings.file_extension_filter),
                    std::end(settings.file_extension_filter),
                    [&filepath](const std::string_view ext)
                    {
                        return filepath.extension() == ext;
                    }
                );

                if (matches_extension && settings.tiled_previews == true)
                {
                    const std::string filepath_utf8 =
                        PathToUtf8(filepath);

                    SDL_Texture* loaded_texture = IMG_LoadTexture(
                        Renderer::s_renderer,
                        filepath_utf8.c_str()
                    );

                    FileSelectorEntry new_entry{
                        filepath,
                        SDLTextureHandle{loaded_texture}
                    };

                    if (new_entry.thumbnail != nullptr)
                    {
                        SDL_SetTextureScaleMode(
                            new_entry.thumbnail.get(),
                            SDL_ScaleMode::SDL_SCALEMODE_NEAREST
                        );
                    }

                    return new_entry;
                }

                return FileSelectorEntry{filepath};
            }
        );
    }

    if (ImGui::IsPopupOpen(popup_title.c_str()))
    {
        ImGui::SetNextWindowPos(
            ImGui::GetMainViewport()->GetCenter(),
            ImGuiCond_Appearing,
            ImVec2(0.5f, 0.5f)
        );
    }

    if (ImGui::BeginPopup(
        popup_title.c_str(),
        ImGuiWindowFlags_Modal
    ))
    {
        if (settings.close_popup)
        {
            s_highlighted_path.reset();
            ImGui::CloseCurrentPopup();
        }

        const std::string target_directory_utf8 =
            PathToUtf8(settings.target_directory);

        ImGui::Text(
            "%s",
            target_directory_utf8.c_str()
        );

        ImGui::Separator();

        if (ImGui::BeginChild(
            "file_list",
            ImVec2{-1, 800}
        ))
        {
            bool has_files = false;
            unsigned int file_index = 0;
            bool group_open = false;

            for (const FileSelectorEntry& file_entry : s_file_entries)
            {
                if (std::none_of(
                    std::begin(settings.file_extension_filter),
                    std::end(settings.file_extension_filter),
                    [&file_entry](const std::string_view ext)
                    {
                        return file_entry.filepath.extension() == ext;
                    }
                ))
                {
                    continue;
                }

                const bool is_selected =
                    s_highlighted_path == file_entry.filepath;

                has_files = true;

                if (file_entry.thumbnail != nullptr)
                {
                    if ((file_index % settings.num_columns) == 0)
                    {
                        ImGui::BeginGroup();
                        group_open = true;
                    }
                    else
                    {
                        ImGui::SameLine();
                    }

                    ImGui::BeginGroup();

                    {
                        constexpr float ideal_scale = 2.0f;
                        constexpr ImVec2 ideal_dimensions{128, 128};

                        ImVec2 dimensions{
                            static_cast<float>(
                                file_entry.thumbnail->w
                            ),
                            static_cast<float>(
                                file_entry.thumbnail->h
                            )
                        };

                        if (dimensions.x >= dimensions.y)
                        {
                            if (dimensions.x < ideal_dimensions.x)
                            {
                                dimensions *= ideal_scale;
                            }

                            if (dimensions.x > ideal_dimensions.x)
                            {
                                dimensions =
                                    dimensions *
                                    (
                                        ideal_dimensions.x /
                                        dimensions.x
                                    );
                            }
                        }
                        else
                        {
                            if (dimensions.x < ideal_dimensions.y)
                            {
                                dimensions *= ideal_scale;
                            }

                            if (dimensions.y > ideal_dimensions.y)
                            {
                                dimensions =
                                    dimensions *
                                    (
                                        ideal_dimensions.y /
                                        dimensions.y
                                    );
                            }
                        }

                        const float cursor_pos_x =
                            ImGui::GetCursorPosX();

                        ImGui::SetCursorPosX(
                            cursor_pos_x +
                            (
                                ideal_dimensions.x -
                                dimensions.x
                            ) * 0.5f
                        );

                        ImGui::Image(
                            reinterpret_cast<ImTextureID>(
                                file_entry.thumbnail.get()
                            ),
                            dimensions
                        );

                        const std::string filename_utf8 =
                            PathToUtf8(
                                file_entry.filepath.filename()
                            );

                        ImGui::PushTextWrapPos(
                            cursor_pos_x +
                            ideal_dimensions.x
                        );

                        ImGui::TextWrapped(
                            "%s",
                            filename_utf8.c_str()
                        );

                        ImGui::PopTextWrapPos();
                    }

                    ImGui::EndGroup();

                    if (is_selected)
                    {
                        ImGui::GetCurrentWindow()
                            ->DrawList
                            ->AddRect(
                                ImGui::GetItemRectMin() -
                                    ImVec2{1, 1},
                                ImGui::GetItemRectMax() +
                                    ImVec2{1, 1},
                                ImGui::GetColorU32(
                                    ImVec4{
                                        1.0f,
                                        0.0f,
                                        1.0f,
                                        1.0f
                                    }
                                ),
                                0.0f,
                                ImDrawFlags_None,
                                2.0f
                            );
                    }

                    ++file_index;
                }
                else
                {
                    bool selected = is_selected;

                    const std::string filename_utf8 =
                        PathToUtf8(
                            file_entry.filepath.filename()
                        );

                    ImGui::Selectable(
                        filename_utf8.c_str(),
                        &selected,
                        ImGuiSelectableFlags_DontClosePopups
                    );
                }

                if (ImGui::IsItemClicked(
                    ImGuiMouseButton_Left
                ))
                {
                    s_highlighted_path =
                        file_entry.filepath;
                }

                if (
                    ImGui::IsItemClicked(
                        ImGuiMouseButton_Left
                    ) &&
                    ImGui::IsMouseDoubleClicked(
                        ImGuiMouseButton_Left
                    )
                )
                {
                    return_path = s_highlighted_path;
                }

                if (
                    group_open &&
                    (file_index % settings.num_columns) == 0
                )
                {
                    ImGui::EndGroup();
                    group_open = false;
                }
            }

            if (group_open)
            {
                ImGui::EndGroup();
                group_open = false;
            }

            if (has_files == false)
            {
                ImGui::Text(
                    "<No valid %s detected>",
                    settings.object_typename.c_str()
                );

                const std::filesystem::path path_diff =
                    std::filesystem::relative(
                        settings.target_directory,
                        std::filesystem::current_path()
                    );

                const std::string path_diff_utf8 =
                    PathToUtf8(path_diff);

                ImGui::Text(
                    "<Add to the %s folder in the executable directory>",
                    path_diff_utf8.c_str()
                );
            }
        }

        ImGui::EndChild();
        ImGui::Separator();

        ImGui::BeginDisabled(
            s_highlighted_path.has_value() == false
        );

        const std::string selection_str =
            "Choose " + settings.object_typename;

        if (
            ImGui::Button(selection_str.c_str()) &&
            s_highlighted_path.has_value()
        )
        {
            return_path = s_highlighted_path;
        }

        ImGui::EndDisabled();
        ImGui::SameLine();

        if (ImGui::Button("Cancel"))
        {
            s_highlighted_path.reset();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    return return_path;
}

}
