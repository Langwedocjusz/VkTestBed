#include "AssetManager.h"

#include "ModelConfig.h"
#include "ModelLoader.h"

#include <mutex>
#include <ranges>

#include <iostream>

AssetManager::AssetManager(Scene &scene) : mScene(scene)
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
        EmplaceThings();

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
        // To-do: data should probably not be passed by value into the lambda
        for (auto &data : mModel.ImgData)
        {
            mThreadPool.Push([this, &data]() {
                auto &img = mScene.Images[data.ImageKey];

                img = ImageData::ImportSTB(data.Path, data.Unorm);

                mModel.TasksLeft--;
            });
        }

        // 3.2. Schedule mesh parsing:
        for (auto &data : mModel.PrimData)
        {
            mThreadPool.Push([this, &data]() {
                auto &mesh = mScene.Meshes[data.SceneMesh];
                auto &prim = mesh.Geometry[data.ScenePrim];

                auto &srcMesh = mModel.Gltf->meshes[data.GltfMesh];
                auto &srcPrimitive = srcMesh.primitives[data.GltfPrim];

                prim =
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
    auto SyncedEmplaceImage = [this]() {
        std::unique_lock lock(mSceneMutex);
        return mScene.EmplaceImage();
    };

    mThreadPool.Push([this, SyncedEmplaceImage, path]() {
        auto [imgKey, img] = SyncedEmplaceImage();

        img = ImageData::ImportEXR(path.string());

        mScene.Env.HdriImage = imgKey;

        {
            std::unique_lock lk(mSceneMutex);
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

void AssetManager::EmplaceThings()
{
    using namespace std::views;

    const std::filesystem::path workingDir = mModel.Config.Filepath.parent_path();

    auto SyncedEmplaceMesh = [this]() {
        std::unique_lock lock(mSceneMutex);
        return mScene.EmplaceMesh();
    };

    auto SyncedEmplaceImage = [this]() {
        std::unique_lock lock(mSceneMutex);
        return mScene.EmplaceImage();
    };

    auto SyncedEmplaceMaterial = [this]() {
        std::unique_lock lock(mSceneMutex);
        return mScene.EmplaceMaterial();
    };

    // Create the new mesh:
    auto [meshKey, mesh] = SyncedEmplaceMesh();

    mesh.Name = mModel.Config.Filepath.stem().string();

    // Gather ids of the mesh materials:
    std::map<size_t, SceneKey> keyMap;

    // Loop over all materials in gltf:
    for (auto [id, material] : enumerate(mModel.Gltf->materials))
    {
        // Create new scene material:
        auto [matKey, mat] = SyncedEmplaceMaterial();
        mat.Name = mesh.Name + std::to_string(id);

        keyMap[id] = matKey;

        // Load alpha cutoff where applicable
        if (material.alphaMode == fastgltf::AlphaMode::Mask)
        {
            mat.AlphaCutoff = material.alphaCutoff;
        }

        // Handle albedo:
        {
            auto [imgKey, img] = SyncedEmplaceImage();
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

                img = ImageData::SinglePixel(Pixel{r, g, b, a});
            }
        }

        // Do the same for roughness map if requested:
        if (mModel.Config.FetchRoughness)
        {
            auto &roughnessInfo = material.pbrData.metallicRoughnessTexture;
            auto roughnessPath = GetTexturePath(*mModel.Gltf, roughnessInfo, workingDir);

            auto [imgKey, img] = SyncedEmplaceImage();
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
                auto &img = mScene.Images[imgKey];

                const auto m =
                    static_cast<uint8_t>(255.0f * material.pbrData.metallicFactor);
                const auto r =
                    static_cast<uint8_t>(255.0f * material.pbrData.roughnessFactor);

                img = ImageData::SinglePixel(Pixel{0, r, m, 0});
            }
        }

        // Do the same for normal map if requested:
        if (mModel.Config.FetchNormal)
        {
            auto &normalInfo = material.normalTexture;
            auto normalPath = GetTexturePath(*mModel.Gltf, normalInfo, workingDir);

            if (normalPath.has_value())
            {
                auto [imgKey, img] = SyncedEmplaceImage();

                mat.Normal = imgKey;

                mModel.ImgData.push_back(ImageTaskData{
                    .ImageKey = imgKey,
                    .Path = *normalPath,
                    .Unorm = true,
                });
            }
        }
    }

    // Iterate all mesh primitives:
    for (auto [submeshId, submesh] : enumerate(mModel.Gltf->meshes))
    {
        for (auto [primId, prim] : enumerate(submesh.primitives))
        {
            // Assign material keys to the mesh:
            if (auto id = prim.materialIndex)
            {
                auto matId = keyMap[*id];

                mesh.Materials.push_back(matId);
            }

            mesh.Geometry.emplace_back();

            mModel.PrimData.push_back(PrimitiveTaskData{
                .SceneMesh = meshKey,
                .ScenePrim = mesh.Geometry.size() - 1,
                .GltfMesh = submeshId,
                .GltfPrim = primId,
            });
        }
    }

    // Set the number of tasks to do:
    mModel.TasksLeft =
        static_cast<int64_t>(mModel.ImgData.size() + mModel.PrimData.size());
}
