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

AssetManager::AssetManager(Scene &scene, SceneEditor &editor)
    : mScene(scene), mEditor(editor)
{
    (void)mEditor;
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

                img = ImageData::ImportSTB(data.Path.string(), data.Unorm);

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
        auto [imgKey, img] = mScene.EmplaceImage();

        img = ImageData::ImportEXR(path.string());

        mScene.Env.HdriImage = imgKey;

        mScene.RequestUpdate(Scene::UpdateFlag::Images);
        mScene.RequestUpdate(Scene::UpdateFlag::Environment);
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

            // Check if there is an albedo texture, if so - emplace its image:
            auto &albedoInfo = material.pbrData.baseColorTexture;
            auto albedoPath = GetTexturePath(*mModel.Gltf, albedoInfo, workingDir);

            if (albedoPath.has_value())
            {
                mModel.ImgData.push_back(ImageTaskData{
                    .ImageKey = imgKey,
                    .Path = *albedoPath,
                    .Unorm = false,
                });
            }

            else
            {
                auto &fac = material.pbrData.baseColorFactor;

                const auto r = static_cast<uint8_t>(255.0f * fac.x());
                const auto g = static_cast<uint8_t>(255.0f * fac.y());
                const auto b = static_cast<uint8_t>(255.0f * fac.z());
                const auto a = static_cast<uint8_t>(255.0f * fac.w());

                img = ImageData::SinglePixel(Pixel{.R = r, .G = g, .B = b, .A = a});
            }
        }

        // Do the same for roughness map if requested:
        if (mModel.Config.FetchRoughness)
        {
            auto &roughnessInfo = material.pbrData.metallicRoughnessTexture;
            auto roughnessPath = GetTexturePath(*mModel.Gltf, roughnessInfo, workingDir);

            auto [imgKey, img] = mScene.EmplaceImage();
            mat.Roughness = imgKey;

            if (roughnessPath.has_value())
            {
                mModel.ImgData.push_back(ImageTaskData{
                    .ImageKey = imgKey,
                    .Path = *roughnessPath,
                    .Unorm = true,
                });
            }

            else
            {
                const auto m =
                    static_cast<uint8_t>(255.0f * material.pbrData.metallicFactor);
                const auto r =
                    static_cast<uint8_t>(255.0f * material.pbrData.roughnessFactor);

                img = ImageData::SinglePixel(Pixel{.R = 0, .G = r, .B = m, .A = 0});
            }
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
                    .Path = *normalPath,
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

        // Update mesh dictionary:
        mModel.MeshDict[gltfMeshId] = meshKey;

        mesh.Name = baseName;
        mesh.Name += " ";
        mesh.Name += gltfMesh.name;

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
    glm::vec3 translation;
    glm::vec3 rotation;
    glm::vec3 scale;

    auto UnpackTRS = [&](fastgltf::TRS &trs) {
        translation =
            glm::vec3{trs.translation.x(), trs.translation.y(), trs.translation.z()};

        auto quat = glm::quat(trs.rotation.w(), trs.rotation.x(), trs.rotation.y(),
                              trs.rotation.z());

        rotation = glm::eulerAngles(quat);

        scale = glm::vec3{trs.scale.x(), trs.scale.y(), trs.scale.z()};
    };

    auto UnpackMat4 = [&](fastgltf::math::fmat4x4 &mat) {
        fastgltf::TRS trs;
        fastgltf::math::decomposeTransformMatrix(mat, trs.scale, trs.rotation,
                                                 trs.translation);

        UnpackTRS(trs);
    };

    std::visit(overloaded{[&](fastgltf::TRS arg) { UnpackTRS(arg); },
                          [&](fastgltf::math::fmat4x4 arg) { UnpackMat4(arg); }},
               node.transform);

    return {translation, rotation, scale};
}

void AssetManager::ProcessGltfHierarchy()
{
    // Append root of the hierarchy to scene editor prefabs:
    auto &root = mEditor.Prefabs.emplace_back(mEditor);

    const std::string baseName = mModel.Config.Filepath.stem().string();
    root.Name = baseName;

    // To-do: Currently we assume gltf holds one scene.
    auto &scene = mModel.Gltf->scenes[0];

    for (auto nodeIdx : scene.nodeIndices)
    {
        auto &node = mModel.Gltf->nodes[nodeIdx];
        auto [translation, rotation, scale] = UnpackTransform(node);

        // To-do: Assumes each of the 1st levl nodes holds a mesh.
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