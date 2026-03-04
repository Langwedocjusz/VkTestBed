#include "ModelLoaderGui.h"
#include "Pch.h"

#include "VertexLayout.h"
#include "Vassert.h"

#include "imgui.h"

#include <filesystem>

ModelLoaderGui::ModelLoaderGui(SceneEditor &editor) : mEditor(editor)
{
    auto path = std::filesystem::current_path() / "assets/gltf";

    if (std::filesystem::exists(path))
    {
        mBrowser.CurrentPath = path;
    }

    mBrowser.AddExtensionToFilter(".exr");

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

    mFileMenuOpen   = true;
    mImportMenuOpen = true;
}

void ModelLoaderGui::ImportMenu()
{
    if (ImGui::BeginPopupModal("Import options", &mImportMenuOpen))
    {
        ImGui::Text("Vertex Layout:");
        ImGui::Separator();

        static int32_t    choice = 1;
        static std::array names{"Push", "Pull Naive", "Pull Compressed"};

        bool typeChanged = ImGui::Combo("Type", &choice, names.data(),
                     static_cast<int32_t>(names.size()));

        if (typeChanged)
        {
            if (choice == 0)
            {
                mModelConfig.VertexLayout = Vertex::PushLayout{};
            }
            else if (choice == 1)
            {
                mModelConfig.VertexLayout = Vertex::PullLayout::Naive;
            }
            else if (choice == 2)
            {
                mModelConfig.VertexLayout = Vertex::PullLayout::Compressed;
            }
            else
            {
                vpanic("Unhandlec choice value!");
            }
        }
        
        if (auto* layout = std::get_if<Vertex::PushLayout>(&mModelConfig.VertexLayout))
        {
            bool v = true;
            ImGui::Checkbox("Position", &v);

            ImGui::Checkbox("TexCoord", &layout->HasTexCoord);
            ImGui::Checkbox("Normal", &layout->HasNormal);
            ImGui::Checkbox("Tangent", &layout->HasTangent);
            ImGui::Checkbox("Color", &layout->HasColor);
        }

        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        ImGui::Text("Material Options:");
        ImGui::Separator();

        bool v = true;
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