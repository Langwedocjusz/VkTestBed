#include "ModelLoaderGui.h"

#include "ModelLoader.h"

#include "imgui.h"

#include <filesystem>

void ModelLoaderGui::TriggerLoad(Scene& scene)
{
    mScene = &scene;
    mFilePopup = true;

}

void ModelLoaderGui::ModelLoaderGui::OnImGui()
{
    if(mFilePopup)
    {
        ImGui::OpenPopup("Load...");
        mFilePopup = false;
    }

    if(mImportPopup)
    {
        ImGui::OpenPopup("Import options");
        mImportPopup = false;
    }

    FileMenu();
    ImportMenu();

    mFileMenuOpen = true;
    mImportMenuOpen = true;
}

void ModelLoaderGui::FileMenu()
{
    if (ImGui::BeginPopupModal("Load...", &mFileMenuOpen))
    {
        constexpr size_t maxNameLength = 40;
        const std::string buttonText{"Load"};

        ImGuiStyle &style = ImGui::GetStyle();

        const float buttonWidth = ImGui::CalcTextSize(buttonText.c_str()).x +
                                  2.0f * style.FramePadding.x + style.ItemSpacing.x;
        const float buttonHeight = ImGui::CalcTextSize(buttonText.c_str()).y +
                                   2.0f * style.FramePadding.y + style.ItemSpacing.y;

        mBrowser.OnImGui(buttonHeight);

        const float textWidth = ImGui::GetContentRegionAvail().x - buttonWidth;

        ImGui::PushItemWidth(textWidth);
        ImGui::InputText("##load_filename", mBrowser.ChosenFile.string().data(),
                         maxNameLength, ImGuiInputTextFlags_ReadOnly);
        ImGui::PopItemWidth();

        ImGui::SameLine();

        const bool validTarget = std::filesystem::is_regular_file(mBrowser.ChosenFile);

        if (ImGui::Button(buttonText.c_str()) && validTarget)
        {
            mImportPopup = true;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void ModelLoaderGui::ImportMenu()
{
    if (ImGui::BeginPopupModal("Import options", &mImportMenuOpen))
    {
        ImGui::Text("WORK IN PROGRESS");

        if (ImGui::Button("Load"))
        {
            ImGui::CloseCurrentPopup();

            LoadModel();
        }

        ImGui::EndPopup();
    }
}

void ModelLoaderGui::LoadModel()
{
    assert(mScene != nullptr);

    //Load Geo Providers:
    ModelLoader::Config config{
        .Filepath = mBrowser.ChosenFile,
    };

    mScene->GeoProviders.push_back(ModelLoader::PosTex(config));

    //Load Materials:


    //Create an instance:

    //Set update flags:
    mScene->UpdateRequested = true;
    mScene->GlobalUpdate = true;

    //Reset scene ptr:
    mScene = nullptr;
}