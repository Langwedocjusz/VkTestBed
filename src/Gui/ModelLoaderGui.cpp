#include "ModelLoaderGui.h"

#include "ModelLoader.h"
#include "imgui.h"

#include <fastgltf/types.hpp>
#include <filesystem>
#include <iostream>
#include <ranges>

ModelLoaderGui::ModelLoaderGui(SceneEditor &editor) : mEditor(editor)
{
    mBrowser.AddExtensionToFilter(".gltf");

    mBrowser.SetCallbackFn([&]() { mImportPopup = true; });

    mBrowser.SetCheckFn([](const std::filesystem::path &path) {
        return std::filesystem::is_regular_file(path);
    });
}

void ModelLoaderGui::TriggerLoad()
{
    mFilePopup = true;
}

void ModelLoaderGui::ModelLoaderGui::OnImGui()
{
    // File popup:
    if (mFilePopup)
    {
        ImGui::OpenPopup("Load...");
        mFilePopup = false;
    }

    mBrowser.ImGuiLoadPopup("Load...", mFileMenuOpen);

    // Import popup:
    if (mImportPopup)
    {
        ImGui::OpenPopup("Import options");
        mImportPopup = false;
    }

    ImportMenu();

    mFileMenuOpen = true;
    mImportMenuOpen = true;
}

void ModelLoaderGui::ImportMenu()
{
    if (ImGui::BeginPopupModal("Import options", &mImportMenuOpen))
    {
        ImGui::Text("Vertex Attributes:");
        ImGui::Separator();

        bool v = true;
        ImGui::Checkbox("Position", &v);

        ImGui::Checkbox("TexCoord", &mModelConfig.LoadTexCoord);
        ImGui::Checkbox("Normal", &mModelConfig.LoadNormals);
        ImGui::Checkbox("Tangent", &mModelConfig.LoadTangents);
        ImGui::Checkbox("Color", &mModelConfig.LoadColor);

        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        ImGui::Text("Material Options:");
        ImGui::Separator();

        ImGui::Checkbox("Fetch Albedo", &v);
        ImGui::Checkbox("Fetch Normal", &mModelConfig.FetchNormal);
        ImGui::Checkbox("Fetch Roughness", &mModelConfig.FetchRoughness);

        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        // Final load button:
        auto size = ImVec2(ImGui::GetContentRegionAvail().x, 0.0f);

        if (ImGui::Button("Load", size))
        {
            ImGui::CloseCurrentPopup();

            LoadModel();
        }

        ImGui::EndPopup();
    }
}

void ModelLoaderGui::LoadModel()
{
    mModelConfig.Filepath = mBrowser.ChosenFile;

    mEditor.LoadModel(mModelConfig);
}