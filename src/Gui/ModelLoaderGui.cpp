#include "ModelLoaderGui.h"

#include "ModelLoader.h"

#include "imgui.h"

#include <fastgltf/types.hpp>
#include <filesystem>
#include <iostream>
#include <ranges>

void ModelLoaderGui::TriggerLoad(Scene &scene)
{
    mScene = &scene;
    mFilePopup = true;
}

void ModelLoaderGui::ModelLoaderGui::OnImGui()
{
    if (mFilePopup)
    {
        ImGui::OpenPopup("Load...");
        mFilePopup = false;
    }

    if (mImportPopup)
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

        // Do a preliminary scan of the gltf and retrieve materials etc:

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
    using namespace std::views;

    assert(mScene != nullptr);

    // Get gltf object:
    auto gltf = ModelLoader::GetGltf(mBrowser.ChosenFile);

    // Load Geo Providers:
    auto &newMesh = mScene->Meshes.emplace_back();
    newMesh.Name = mBrowser.ChosenFile.stem().string();
    newMesh.GeoProvider = ModelLoader::LoadModel(mBrowser.ChosenFile);

    // Load Materials:
    size_t matIdOffset = mScene->Materials.size();

    for (auto [id, material] : enumerate(gltf.materials))
    {
        // 1. Does albedo texture exist?
        auto &albedoTex = material.pbrData.baseColorTexture;

        if (!albedoTex.has_value())
            continue;

        // 2. Retrieve image index if present
        auto imgId = gltf.textures[albedoTex->textureIndex].imageIndex;

        if (!imgId.has_value())
            continue;

        // 3. Get data source
        auto &dataSource = gltf.images[imgId.value()].data;

        // 4. Retrieve the URI if present;
        if (!std::holds_alternative<fastgltf::sources::URI>(dataSource))
            continue;

        auto &uri = std::get<fastgltf::sources::URI>(dataSource);

        // 5. Retrieve the filepath:
        auto relPath = uri.uri.fspath();
        auto path = mBrowser.CurrentPath / relPath;

        // 6. Create new scene material, pointing to the albedo texture
        auto &mat = mScene->Materials.emplace_back();
        mat.Name = newMesh.Name + std::to_string(id);

        mat[Material::Albedo] = Material::ImageSource{
            .Path = path,
            .Channel = Material::ImageChannel::RGB,
        };
    }

    for (auto &mesh : gltf.meshes)
    {
        for (auto &primitive : mesh.primitives)
        {
            if (auto id = primitive.materialIndex)
            {
                auto matId = matIdOffset + (*id);

                newMesh.MaterialIds.push_back(matId);
            }
        }
    }

    // Create an instance:

    // Set update flags:
    mScene->UpdateRequested = true;
    mScene->GlobalUpdate = true;

    // Reset scene ptr:
    mScene = nullptr;
}