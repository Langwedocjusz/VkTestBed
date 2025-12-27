#include "AssetManager.h"
#include "Pch.h"

#include "ModelConfig.h"
#include "ModelLoader.h"
#include "ThreadPool.h"
#include "Timer.h"

#include <glm/gtc/quaternion.hpp>

#include <fastgltf/math.hpp>
#include <fastgltf/types.hpp>

#include <atomic>
#include <iostream>
#include <ranges>
#include <variant>

static uint8_t PixelChannelFromFloat(float x)
{
    return static_cast<uint8_t>(255.0f * x);
}

struct AssetManager::Model {
    Model(const ModelConfig &config, bool &isReady)
        : Config(config), IsReady(isReady), TasksLeft(0), StartTime(Timer::Now())
    {
    }

    ModelConfig Config;
    bool &IsReady;
    fastgltf::Asset Gltf;
    std::vector<ImageTaskData> ImgData;
    std::vector<PrimitiveTaskData> PrimData;
    std::map<size_t, SceneKey> MeshDict;
    std::atomic_int64_t TasksLeft;
    Timer::TimePoint StartTime;
};

AssetManager::AssetManager(Scene &scene) : mScene(scene)
{
    mThreadPool = std::make_unique<ThreadPool>();
}

AssetManager::~AssetManager()
{
}

void AssetManager::LoadModel(const ModelConfig &config, SceneGraphNode &root,
                             bool &isReady)
{
    // 0. If already loading do nothing
    if (mModelStage != ModelStage::Idle)
        return;

    // 1. Update the state
    mModelStage = ModelStage::Parsing;
    mModel = std::make_unique<Model>(config, isReady);

    // 2. Launch an async task to parse gltf and emplace new
    // elements in the scene
    mThreadPool->Push([this, &root]() {
        ParseGltf();
        PreprocessGltfAssets();
        ProcessGltfHierarchy(root);

        mModelStage = ModelStage::Parsed;
    });
}

void AssetManager::OnUpdate()
{
    // 3. Detect if there is work to be scheduled
    if (mModelStage == ModelStage::Parsed)
    {
        mModelStage = ModelStage::Loading;

        // 3.1. Schedule image loading:
        for (auto &data : mModel->ImgData)
        {
            mThreadPool->Push([this, &data]() {
                auto &img = mScene.Images[data.ImageKey];

                if (data.Path)
                {
                    auto pathStr = data.Path->string();
                    img = ImageData::ImportSTB(pathStr.c_str(), data.Unorm);
                }
                else
                    img = ImageData::SinglePixel(data.BaseColor, data.Unorm);

                img.Name = data.Name;

                mModel->TasksLeft--;
            });
        }

        // 3.2. Schedule mesh parsing:
        for (auto &data : mModel->PrimData)
        {
            mThreadPool->Push([this, &data]() {
                auto &mesh = mScene.Meshes[data.SceneMesh];
                auto &prim = mesh.Primitives[data.ScenePrim];

                auto &srcMesh = mModel->Gltf.meshes[data.GltfMesh];
                auto &srcPrimitive = srcMesh.primitives[data.GltfPrim];

                prim.Data = ModelLoader::LoadPrimitive(mModel->Gltf, mModel->Config,
                                                       srcPrimitive);

                mModel->TasksLeft--;
            });
        }
    }

    // 4. Detect if loading done
    if (mModelStage == ModelStage::Loading)
    {
        if (mModel->TasksLeft == 0)
        {
            mModelStage = ModelStage::Idle;

            // Set scene update flags:
            mScene.RequestUpdate(Scene::UpdateFlag::Images);
            mScene.RequestUpdate(Scene::UpdateFlag::Meshes);
            mScene.RequestUpdate(Scene::UpdateFlag::Materials);
            mScene.RequestUpdate(Scene::UpdateFlag::MeshMaterials);

            // Mark prefab as ready:
            mModel->IsReady = true;

            // Print message:
            auto now = Timer::Now();
            auto time = Timer::GetDiffSeconds(now, mModel->StartTime);
            std::cout << "Finished loading model (took " << time << " [s])\n";

            // Free task-related memory:
            mModel.reset(nullptr);
        }
    }
}

void AssetManager::ParseGltf()
{
    mModel->Gltf = ModelLoader::GetGltfWithBuffers(mModel->Config.Filepath);
}

void AssetManager::LoadHdri(const std::filesystem::path &path)
{
    mThreadPool->Push([this, path]() {
        if (mHDRI.LastPath != path)
        {
            mHDRI.LastPath = path;

            auto pathStr = path.string();
            mScene.Env.HdriImage = ImageData::ImportEXR(pathStr.c_str());
            mScene.Env.ReloadImage = true;

            mScene.RequestUpdate(Scene::UpdateFlag::Images);
            mScene.RequestUpdate(Scene::UpdateFlag::Environment);
        }
    });
}

void AssetManager::ClearCachedHDRI()
{
    mHDRI.LastPath = std::nullopt;
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

    const std::filesystem::path workingDir = mModel->Config.Filepath.parent_path();
    const std::string baseName = mModel->Config.Filepath.stem().string();

    // Gather ids of the mesh materials:
    std::map<size_t, SceneKey> keyMap;

    // Loop over all materials in gltf:
    for (auto [id, material] : enumerate(mModel->Gltf.materials))
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

        // Load info about double-sidedness:
        mat.DoubleSided = material.doubleSided;

        // Load information about translucency/diffuse transmission:
        if (material.diffuseTransmission)
        {
            auto color = material.diffuseTransmission->diffuseTransmissionColorFactor;

            mat.TranslucentColor = glm::vec3(color.x(), color.y(), color.z());
        }

        // Handle albedo:
        {
            auto [imgKey, img] = mScene.EmplaceImage();
            mat.Albedo = imgKey;

            auto &albedoInfo = material.pbrData.baseColorTexture;
            auto albedoPath = GetTexturePath(mModel->Gltf, albedoInfo, workingDir);

            auto &fac = material.pbrData.baseColorFactor;

            auto baseColor = Pixel{
                .R = PixelChannelFromFloat(fac.x()),
                .G = PixelChannelFromFloat(fac.y()),
                .B = PixelChannelFromFloat(fac.z()),
                .A = PixelChannelFromFloat(fac.w()),
            };

            mModel->ImgData.push_back(ImageTaskData{
                .ImageKey = imgKey,
                .Path = albedoPath,
                .BaseColor = baseColor,
                .Name = mat.Name + " Albedo",
                .Unorm = false,
            });
        }

        // Do the same for roughness/metallic:
        if (mModel->Config.FetchRoughness)
        {
            auto [imgKey, img] = mScene.EmplaceImage();
            mat.Roughness = imgKey;

            auto &roughnessInfo = material.pbrData.metallicRoughnessTexture;
            auto roughnessPath = GetTexturePath(mModel->Gltf, roughnessInfo, workingDir);

            auto baseColor = Pixel{
                .R = PixelChannelFromFloat(0.0f),
                .G = PixelChannelFromFloat(material.pbrData.roughnessFactor),
                .B = PixelChannelFromFloat(material.pbrData.metallicFactor),
                .A = PixelChannelFromFloat(0.0f),
            };

            mModel->ImgData.push_back(ImageTaskData{
                .ImageKey = imgKey,
                .Path = roughnessPath,
                .BaseColor = baseColor,
                .Name = mat.Name + " Roughness",
                .Unorm = true,
            });
        }

        // Do the same for normal map if requested:
        if (mModel->Config.FetchNormal)
        {
            auto &normalInfo = material.normalTexture;
            auto normalPath = GetTexturePath(mModel->Gltf, normalInfo, workingDir);

            if (normalPath.has_value())
            {
                auto [imgKey, img] = mScene.EmplaceImage();
                mat.Normal = imgKey;

                mModel->ImgData.push_back(ImageTaskData{
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
    for (auto [gltfMeshId, gltfMesh] : enumerate(mModel->Gltf.meshes))
    {
        // Create the new mesh:
        auto [meshKey, mesh] = mScene.EmplaceMesh();
        mesh.Name = std::format("{} {}", baseName, gltfMesh.name);

        // Update mesh dictionary:
        mModel->MeshDict[gltfMeshId] = meshKey;

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

            mModel->PrimData.push_back(PrimitiveTaskData{
                .SceneMesh = meshKey,
                .ScenePrim = mesh.Primitives.size() - 1,
                .GltfMesh = gltfMeshId,
                .GltfPrim = gltfPrimId,
            });
        }
    }

    // Set the number of tasks to do:
    mModel->TasksLeft =
        static_cast<int64_t>(mModel->ImgData.size() + mModel->PrimData.size());
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

void AssetManager::ProcessGltfHierarchy(SceneGraphNode &root)
{
    // To-do: Currently we assume gltf holds one scene.
    auto &scene = mModel->Gltf.scenes[0];

    for (auto nodeIdx : scene.nodeIndices)
    {
        auto &node = mModel->Gltf.nodes[nodeIdx];
        auto [translation, rotation, scale] = UnpackTransform(node);

        // To-do: Only handles first-level nodes that hold meshes
        if (node.meshIndex.has_value())
        {
            auto meshKey = mModel->MeshDict[*node.meshIndex];

            auto &prefabNode = root.EmplaceChild(meshKey);
            prefabNode.Translation = translation;
            prefabNode.Rotation = rotation;
            prefabNode.Scale = scale;
            prefabNode.Name = node.name;
        }
    }
}