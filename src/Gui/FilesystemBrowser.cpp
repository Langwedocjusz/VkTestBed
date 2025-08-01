#include "FilesystemBrowser.h"
#include "Pch.h"

// #define IMGUI_DEFINE_MATH_OPERATORS
// #include "imgui_internal.h"

#include "imgui.h"

#include <vector>

FilesystemBrowser::FilesystemBrowser() : CurrentPath(std::filesystem::current_path())
{
}

FilesystemBrowser::FilesystemBrowser(std::filesystem::path currentPath)
    : CurrentPath(std::move(currentPath))
{
}

void FilesystemBrowser::AddExtensionToFilter(const std::string &ext)
{
    if (!mValidExtensions.has_value())
        mValidExtensions = ExtensionSet();

    mValidExtensions->insert(ext);
}

void FilesystemBrowser::ClearExtensionFilter()
{
    mValidExtensions = std::nullopt;
}

void FilesystemBrowser::OnImGuiRaw(float lowerMargin)
{
    // To-do: add icons to make things pretty

    // Parent Directory button
    if (ImGui::Button("Up"))
    {
        CurrentPath = CurrentPath.parent_path();
    }

    ImGui::SameLine();

    // Current filepath display
    const float text_width = ImGui::GetContentRegionAvail().x;
    ImGui::PushItemWidth(text_width);

    std::string filepath = CurrentPath.string();
    ImGui::InputText("##current_directory", filepath.data(), filepath.size(),
                     ImGuiInputTextFlags_ReadOnly);

    ImGui::PopItemWidth();

    // List of subdirectories/files
    const float height = ImGui::GetContentRegionAvail().y - lowerMargin;

    ImGui::BeginChild("#Filesystem browser", ImVec2(0.0f, height), true);

    std::vector<std::filesystem::path> directories, files;

    for (const auto &entry : std::filesystem::directory_iterator(CurrentPath))
    {
        if (std::filesystem::is_directory(entry.path()))
            directories.push_back(entry.path());

        else
            files.push_back(entry.path());
    }

    for (const auto &path : directories)
    {
        const std::string text = "<FOLDER> " + path.filename().string();

        if (ImGui::Selectable(text.c_str()))
            CurrentPath = path;
    }

    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(192, 192, 192, 255));
    for (const auto &path : files)
    {
        if (mValidExtensions.has_value())
        {
            auto ext = path.extension().string();

            if (!mValidExtensions->contains(ext))
            {
                continue;
            }
        }

        const std::string text = "<FILE> " + path.filename().string();

        if (ImGui::Selectable(text.c_str()))
            ChosenFile = path;
    }
    ImGui::PopStyleColor();

    ImGui::EndChild();
}

void FilesystemBrowser::ImGuiLoadPopup(const std::string &name, bool &open)
{
    if (ImGui::BeginPopupModal(name.c_str(), &open))
    {
        constexpr size_t maxNameLength = 40;
        const std::string buttonText{"Load"};

        ImGuiStyle &style = ImGui::GetStyle();

        const float buttonWidth = ImGui::CalcTextSize(buttonText.c_str()).x +
                                  2.0f * style.FramePadding.x + style.ItemSpacing.x;
        const float buttonHeight = ImGui::CalcTextSize(buttonText.c_str()).y +
                                   2.0f * style.FramePadding.y + style.ItemSpacing.y;

        // const ImVec2 buttonSize = ImGui::CalcTextSize(buttonText.c_str()) + 2.0f *
        // style.FramePadding + style.ItemInnerSpacing;

        OnImGuiRaw(buttonHeight);

        const float textWidth = ImGui::GetContentRegionAvail().x - buttonWidth;

        ImGui::PushItemWidth(textWidth);
        ImGui::InputText("##load_filename", ChosenFile.string().data(), maxNameLength,
                         ImGuiInputTextFlags_ReadOnly);
        ImGui::PopItemWidth();

        ImGui::SameLine();

        const bool validTarget = [&]() {
            if (mCheck)
                return mCheck(ChosenFile);
            else
                return true;
        }();

        if (ImGui::Button(buttonText.c_str()) && validTarget)
        {
            mCallback();

            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}