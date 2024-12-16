#include "ModelLoaderGui.h"

#include "ModelLoader.h"
#include "imgui.h"

#include <fastgltf/types.hpp>
#include <filesystem>
#include <iostream>
#include <ranges>

ModelLoaderGui::ModelLoaderGui()
{
    mBrowser.AddExtensionToFilter(".gltf");
}

void ModelLoaderGui::TriggerLoad()
{
    mFilePopup = true;
}

void ModelLoaderGui::ModelLoaderGui::OnImGui(Scene &scene)
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
    ImportMenu(scene);

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

void ModelLoaderGui::ImportMenu(Scene &scene)
{
    if (ImGui::BeginPopupModal("Import options", &mImportMenuOpen))
    {
        ImGui::Text("Vertex Attributes:");
        ImGui::Separator();

        bool v = true;
        ImGui::Checkbox("Position", &v);

        ImGui::Checkbox("TexCoord", &mVertexConfig.LoadTexCoord);
        ImGui::Checkbox("Normal", &mVertexConfig.LoadNormals);
        ImGui::Checkbox("Color", &mVertexConfig.LoadColor);

        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        ImGui::Text("Material Options:");
        ImGui::Separator();

        ImGui::Checkbox("Fetch Albedo", &v);
        ImGui::Checkbox("Fetch Normal", &mMatrialConfig.FetchNormal);
        ImGui::Checkbox("Fetch Roughness", &mMatrialConfig.FetchRoughness);

        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        // Final load button:
        auto size = ImVec2(ImGui::GetContentRegionAvail().x, 0.0f);

        if (ImGui::Button("Load", size))
        {
            ImGui::CloseCurrentPopup();

            LoadModel(scene);
        }

        ImGui::EndPopup();
    }
}

void ModelLoaderGui::LoadModel(Scene &scene)
{
    using namespace std::views;

    // Get gltf object:
    auto gltf = ModelLoader::GetGltf(mBrowser.ChosenFile);

    // Load Geo Providers:
    auto &newMesh = scene.Meshes.emplace_back();
    newMesh.Name = mBrowser.ChosenFile.stem().string();

    auto config = ModelLoader::ModelConfig{
        .Filepath = mBrowser.ChosenFile,
        .Vertex = mVertexConfig,
    };

    newMesh.GeoProvider = ModelLoader::LoadModel(config);

    // Load Materials:
    size_t matIdOffset = scene.Materials.size();

    for (auto [id, material] : enumerate(gltf.materials))
    {
        // Check if there is an albedo texture, skip otherwise
        auto &albedoInfo = material.pbrData.baseColorTexture;

        auto albedoPath = GetTexturePath(gltf, albedoInfo);

        if (!albedoPath.has_value())
            continue;

        // Create new scene material
        auto &mat = scene.Materials.emplace_back();
        mat.Name = newMesh.Name + std::to_string(id);

        // Supply albedo texture info
        mat[Material::Albedo] = Material::ImageSource{
            .Path = albedoPath.value(),
            .Channel = Material::ImageChannel::RGBA,
        };

        // Load alpha cutoff where applicable
        if (material.alphaMode == fastgltf::AlphaMode::Mask)
        {
            mat[Material::AlphaCutoff] = material.alphaCutoff;
        }

        // Load Normalmap if requested:
        if (mMatrialConfig.FetchNormal)
        {
            auto &normalInfo = material.normalTexture;

            auto normalPath = GetTexturePath(gltf, normalInfo);

            if (normalPath.has_value())
            {
                mat[Material::Normal] = Material::ImageSource{
                    .Path = normalPath.value(),
                    .Channel = Material::ImageChannel::RGB,
                };
            }
        }

        // Load roughness map if requested:
        if (mMatrialConfig.FetchNormal)
        {
            auto &roughnessInfo = material.pbrData.metallicRoughnessTexture;

            auto roughnessPath = GetTexturePath(gltf, roughnessInfo);

            if (roughnessPath.has_value())
            {
                mat[Material::Roughness] = Material::ImageSource{
                    .Path = roughnessPath.value(),
                    .Channel = Material::ImageChannel::GB,
                };
            }
        }
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

    scene.RequestMaterialUpdate();
    scene.RequestGeometryUpdate();
}