#include "Pch.h"
#include "AssetManager.h"

#include "CppUtils.h"
#include "ModelConfig.h"
#include "ModelLoader.h"
#include "SceneEditor.h"
#include "glm/gtc/quaternion.hpp"

#include <fastgltf/math.hpp>
#include <fastgltf/types.hpp>
#include <ranges>

#include <iostream>
#include <variant>

static uint8_t PixelChannelFromFloat(float x)
{
    return static_cast<uint8_t>(255.0f * x);
}

AssetManager::AssetManager(Scene &scene, SceneEditor &editor)
    : mScene(scene), mEditor(editor)
{
}

void AssetManager::LoadModel(const ModelConfig &config)
{
    // 0. If already loading do nothing
    // To-do: actually communicate with the ui about this
    if (mModel.Stage != ModelStage::Idle)
        return;

    // 1. Update the state
    mModel.Config = config;
    mModel.Stage = ModelStage::Parsing;
    mModel.mStartTime = Timer::Get();

    // 2. Launch an async task to parse gltf and emplace new
    // elements in the scene
    mThreadPool.Push([this]() {
        ParseGltf();
        PreprocessGltfAssets();
        ProcessGltfHierarchy();

        mModel.Stage = ModelStage::Parsed;
    });
}

void AssetManager::OnUpdate()
{
    // 3. Detect if there is work to be scheduled
    if (mModel.Stage == ModelStage::Parsed)
    {
        mModel.Stage = ModelStage::Loading;

        // 3.1. Schedule image loading:
        for (auto &data : mModel.ImgData)
        {
            mThreadPool.Push([this, &data]() {
                auto &img = mScene.Images[data.ImageKey];

                if (data.Path)
                    img = ImageData::ImportSTB(*data.Path, data.Unorm);
                else
                    img = ImageData::SinglePixel(data.BaseColor, data.Unorm);

                img.Name = data.Name;

                mModel.TasksLeft--;
            });
        }

        // 3.2. Schedule mesh parsing:
        for (auto &data : mModel.PrimData)
        {
            mThreadPool.Push([this, &data]() {
                auto &mesh = mScene.Meshes[data.SceneMesh];
                auto &prim = mesh.Primitives[data.ScenePrim];

                auto &srcMesh = mModel.Gltf->meshes[data.GltfMesh];
                auto &srcPrimitive = srcMesh.primitives[data.GltfPrim];

                prim.Data =
                    ModelLoader::LoadPrimitive(*mModel.Gltf, mModel.Config, srcPrimitive);

                mModel.TasksLeft--;
            });
        }
    }

    // 4. Detect if loading done
    if (mModel.Stage == ModelStage::Loading)
    {
        if (mModel.TasksLeft == 0)
        {
            mModel.Stage = ModelStage::Idle;

            // Free task-related memory:
            mModel.Gltf = nullptr;
            mModel.ImgData.clear();
            mModel.PrimData.clear();
            // mModel.MeshDict.clear();

            // Set scene update flags:
            mScene.RequestUpdate(Scene::UpdateFlag::Images);
            mScene.RequestUpdate(Scene::UpdateFlag::Meshes);
            mScene.RequestUpdate(Scene::UpdateFlag::Materials);
            mScene.RequestUpdate(Scene::UpdateFlag::MeshMaterials);

            // Print message:
            auto time = Timer::GetDiffSeconds(mModel.mStartTime);
            std::cout << "Finished loading model (took " << time << " [s])\n";
        }
    }
}

void AssetManager::ParseGltf()
{
    mModel.Gltf = ModelLoader::GetGltfWithBuffers(mModel.Config.Filepath);
}

void AssetManager::LoadHdri(const std::filesystem::path &path)
{
    mThreadPool.Push([this, path]() {
        if (mImagePathCache.count(path) != 0)
        {
            mScene.Env.HdriImage = mImagePathCache[path];
            mScene.RequestUpdate(Scene::UpdateFlag::Environment);
        }

        else
        {
            auto [imgKey, img] = mScene.EmplaceImage();

            img = ImageData::ImportEXR(path);
            mImagePathCache[path] = imgKey;

            mScene.Env.HdriImage = imgKey;

            mScene.RequestUpdate(Scene::UpdateFlag::Images);
            mScene.RequestUpdate(Scene::UpdateFlag::Environment);
        }
    });
}

// Templated, because it handles several types
// from fastgltf
template <typename T>
static auto GetTexturePath(fastgltf::Asset &gltf, std::optional<T> &texInfo,
                           const std::filesystem::path &workingDir)
    -> std::optional<std::filesystem::path>
{
    // 1. Check if info has value
    if (!texInfo.has_value())
        return std::nullopt;

    // 2. Retrieve image index if present
    auto imgId = gltf.textures[texInfo->textureIndex].imageIndex;

    if (!imgId.has_value())
        return std::nullopt;

    // 3. Get data source
    auto &dataSource = gltf.images[imgId.value()].data;

    // 4. Retrieve the URI if present;
    if (!std::holds_alternative<fastgltf::sources::URI>(dataSource))
        return std::nullopt;

    auto &uri = std::get<fastgltf::sources::URI>(dataSource);

    // 5. Retrieve the filepath:
    return workingDir / uri.uri.fspath();
}

void AssetManager::PreprocessGltfAssets()
{
    using namespace std::views;

    const std::filesystem::path workingDir = mModel.Config.Filepath.parent_path();
    const std::string baseName = mModel.Config.Filepath.stem().string();

    // Gather ids of the mesh materials:
    std::map<size_t, SceneKey> keyMap;

    // Loop over all materials in gltf:
    for (auto [id, material] : enumerate(mModel.Gltf->materials))
    {
        // Create new scene material:
        auto [matKey, mat] = mScene.EmplaceMaterial();
        mat.Name = baseName + std::to_string(id);

        keyMap[id] = matKey;

        // Load alpha cutoff where applicable
        if (material.alphaMode == fastgltf::AlphaMode::Mask)
        {
            mat.AlphaCutoff = material.alphaCutoff;
        }

        // Handle albedo:
        {
            auto [imgKey, img] = mScene.EmplaceImage();
            mat.Albedo = imgKey;

            auto &albedoInfo = material.pbrData.baseColorTexture;
            auto albedoPath = GetTexturePath(*mModel.Gltf, albedoInfo, workingDir);

            auto &fac = material.pbrData.baseColorFactor;

            auto baseColor = Pixel{
                .R = PixelChannelFromFloat(fac.x()),
                .G = PixelChannelFromFloat(fac.y()),
                .B = PixelChannelFromFloat(fac.z()),
                .A = PixelChannelFromFloat(fac.w()),
            };

            mModel.ImgData.push_back(ImageTaskData{
                .ImageKey = imgKey,
                .Path = albedoPath,
                .BaseColor = baseColor,
                .Name = mat.Name + " Albedo",
                .Unorm = false,
            });
        }

        // Do the same for roughness/metallic:
        if (mModel.Config.FetchRoughness)
        {
            auto [imgKey, img] = mScene.EmplaceImage();
            mat.Roughness = imgKey;

            auto &roughnessInfo = material.pbrData.metallicRoughnessTexture;
            auto roughnessPath = GetTexturePath(*mModel.Gltf, roughnessInfo, workingDir);

            auto baseColor = Pixel{
                .R = PixelChannelFromFloat(0.0f),
                .G = PixelChannelFromFloat(material.pbrData.roughnessFactor),
                .B = PixelChannelFromFloat(material.pbrData.metallicFactor),
                .A = PixelChannelFromFloat(0.0f),
            };

            mModel.ImgData.push_back(ImageTaskData{
                .ImageKey = imgKey,
                .Path = roughnessPath,
                .BaseColor = baseColor,
                .Name = mat.Name + " Roughness",
                .Unorm = true,
            });
        }

        // Do the same for normal map if requested:
        if (mModel.Config.FetchNormal)
        {
            auto &normalInfo = material.normalTexture;
            auto normalPath = GetTexturePath(*mModel.Gltf, normalInfo, workingDir);

            if (normalPath.has_value())
            {
                auto [imgKey, img] = mScene.EmplaceImage();
                mat.Normal = imgKey;

                mModel.ImgData.push_back(ImageTaskData{
                    .ImageKey = imgKey,
                    .Path = normalPath,
                    .BaseColor = Pixel{},
                    .Name = mat.Name + " Normal",
                    .Unorm = true,
                });
            }
        }
    }

    // Iterate all gltf meshes:
    for (auto [gltfMeshId, gltfMesh] : enumerate(mModel.Gltf->meshes))
    {
        // Create the new mesh:
        auto [meshKey, mesh] = mScene.EmplaceMesh();
        mesh.Name = std::format("{} {}", baseName, gltfMesh.name);

        // Update mesh dictionary:
        mModel.MeshDict[gltfMeshId] = meshKey;

        // Retrieve its primitives:
        for (auto [gltfPrimId, gltfPrim] : enumerate(gltfMesh.primitives))
        {
            // Emplace new primitive:
            auto &newMeshPrim = mesh.Primitives.emplace_back();

            // Assign material keys to the mesh:
            if (auto id = gltfPrim.materialIndex)
            {
                auto matId = keyMap[*id];

                newMeshPrim.Material = matId;
            }

            mModel.PrimData.push_back(PrimitiveTaskData{
                .SceneMesh = meshKey,
                .ScenePrim = mesh.Primitives.size() - 1,
                .GltfMesh = gltfMeshId,
                .GltfPrim = gltfPrimId,
            });
        }
    }

    // Set the number of tasks to do:
    mModel.TasksLeft =
        static_cast<int64_t>(mModel.ImgData.size() + mModel.PrimData.size());
}

static auto UnpackTransform(fastgltf::Node &node)
    -> std::tuple<glm::vec3, glm::vec3, glm::vec3>
{
    fastgltf::TRS trs;

    if (std::holds_alternative<fastgltf::TRS>(node.transform))
    {
        trs = std::get<fastgltf::TRS>(node.transform);
    }
    else
    {
        auto &mat = std::get<fastgltf::math::fmat4x4>(node.transform);
        fastgltf::math::decomposeTransformMatrix(mat, trs.scale, trs.rotation,
                                                 trs.translation);
    }

    auto &t = trs.translation;
    auto &r = trs.rotation;
    auto &s = trs.scale;

    auto translation = glm::vec3(t.x(), t.y(), t.z());

    auto quat = glm::quat(r.w(), r.x(), r.y(), r.z());
    auto rotation = glm::eulerAngles(quat);

    auto scale = glm::vec3(s.x(), s.y(), s.z());

    return {translation, rotation, scale};
}

void AssetManager::ProcessGltfHierarchy()
{
    // Append root of the hierarchy to scene editor prefabs:
    auto [_, root] = mEditor.EmplacePrefab();
    root.Name = mModel.Config.Filepath.stem().string();

    // To-do: Currently we assume gltf holds one scene.
    auto &scene = mModel.Gltf->scenes[0];

    for (auto nodeIdx : scene.nodeIndices)
    {
        auto &node = mModel.Gltf->nodes[nodeIdx];
        auto [translation, rotation, scale] = UnpackTransform(node);

        // To-do: Only handles first-level nodes that hold meshes
        if (node.meshIndex.has_value())
        {
            auto meshKey = mModel.MeshDict[*node.meshIndex];

            auto &prefabNode = root.EmplaceChild(meshKey);
            prefabNode.Translation = translation;
            prefabNode.Rotation = rotation;
            prefabNode.Scale = scale;
            prefabNode.Name = node.name;
        }
    }
}