#include "FilesystemBrowser.h"
#include "Pch.h"

#include "Path.h"

// #define IMGUI_DEFINE_MATH_OPERATORS
// #include "imgui_internal.h"
#include "imgui.h"

#include <vector>

FilesystemBrowser::FilesystemBrowser() : CurrentPath(Path::Current().String())
{
}

FilesystemBrowser::FilesystemBrowser(const std::string &currentPath)
    : CurrentPath(currentPath)
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
    // TODO: add icons to make things pretty

    // Parent Directory button
    if (ImGui::Button("Up"))
    {
        CurrentPath = Path(CurrentPath).Parent().String();
    }

    ImGui::SameLine();

    // Current filepath display
    const float text_width = ImGui::GetContentRegionAvail().x;
    ImGui::PushItemWidth(text_width);

    ImGui::InputText("##current_directory", CurrentPath.data(), CurrentPath.size(),
                     ImGuiInputTextFlags_ReadOnly);

    ImGui::PopItemWidth();

    // List of subdirectories/files
    const float height = ImGui::GetContentRegionAvail().y - lowerMargin;

    ImGui::BeginChild("#Filesystem browser", ImVec2(0.0f, height), true);

    std::vector<Path> directories, files;

    for (auto entry : DirectoryRange(CurrentPath))
    {
        if (entry.IsDirectory())
            directories.push_back(std::move(entry));
        else
            files.push_back(std::move(entry));
    }

    for (const auto &path : directories)
    {
        const std::string text = "<FOLDER> " + path.Filename();

        if (ImGui::Selectable(text.c_str()))
            CurrentPath = path.String();
    }

    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(192, 192, 192, 255));
    for (const auto &path : files)
    {
        if (mValidExtensions.has_value())
        {
            if (!mValidExtensions->contains(path.Extension()))
            {
                continue;
            }
        }

        const std::string text = "<FILE> " + path.Filename();

        if (ImGui::Selectable(text.c_str()))
            ChosenFile = path.String();
    }
    ImGui::PopStyleColor();

    ImGui::EndChild();
}

void FilesystemBrowser::ImGuiLoadPopup(const std::string &name, bool &open)
{
    if (ImGui::BeginPopupModal(name.c_str(), &open))
    {
        constexpr size_t  maxNameLength = 40;
        const std::string buttonText{"Load"};

        ImGuiStyle &style = ImGui::GetStyle();

        const float buttonWidth  = ImGui::CalcTextSize(buttonText.c_str()).x +
                                   2.0f * style.FramePadding.x + style.ItemSpacing.x;
        const float buttonHeight = ImGui::CalcTextSize(buttonText.c_str()).y +
                                   2.0f * style.FramePadding.y + style.ItemSpacing.y;

        // const ImVec2 buttonSize = ImGui::CalcTextSize(buttonText.c_str()) + 2.0f *
        // style.FramePadding + style.ItemInnerSpacing;

        OnImGuiRaw(buttonHeight);

        const float textWidth = ImGui::GetContentRegionAvail().x - buttonWidth;

        ImGui::PushItemWidth(textWidth);

        ImGui::InputText("##load_filename", ChosenFile.data(), maxNameLength,
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