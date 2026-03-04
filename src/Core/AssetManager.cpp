#include "AssetManager.h"
#include "GltfImporter.h"
#include "Pch.h"

#include "ModelConfig.h"
#include "ThreadPool.h"
#include "Timer.h"
#include "VertexPacking.h"

#include <atomic>
#include <memory>
#include <iostream>

struct AssetManager::Model {
    Model(const ModelConfig &config, bool &isReady)
        : Config(config), IsReady(isReady), TasksLeft(0), StartTime(Timer::Now())
    {
    }

    ModelConfig                    Config;
    bool                          &IsReady;
    std::unique_ptr<GltfAsset>     Gltf;
    std::vector<ImageTaskData>     ImgTasks;
    std::vector<PrimitiveTaskData> PrimTasks;
    std::map<size_t, SceneKey>     MeshKeyMap;
    std::atomic_int64_t            TasksLeft;
    Timer::TimePoint               StartTime;
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
    mModel      = std::make_unique<Model>(config, isReady);

    // 2. Launch an async task to parse gltf and emplace new
    // elements in the scene
    mThreadPool->Push([this, &root]() {
        PreprocessGltf(root);
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
        for (auto &data : mModel->ImgTasks)
        {
            mThreadPool->Push([this, &data]() {
                auto &img = mScene.Images[data.ImageKey];

                if (data.Path)
                {
                    auto pathStr = data.Path->string();
                    img          = ImageData::ImportImage(pathStr.c_str(), data.Unorm);
                }
                else
                    img = ImageData::SinglePixel(data.BaseColor, data.Unorm);

                img.Name = data.Name;

                mModel->TasksLeft--;
            });
        }

        // 3.2. Schedule mesh parsing:
        for (auto &data : mModel->PrimTasks)
        {
            mThreadPool->Push([this, &data]() {
                auto &mesh = mScene.Meshes[data.SceneMesh];
                auto &prim = mesh.Primitives[data.ScenePrim];                
                
                auto primDataRaw = mModel->Gltf->LoadPrimitive(data, mModel->Config);
                
                prim.Data = VertexPacking::Encode(primDataRaw, mModel->Config.VertexLayout);

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
            auto now  = Timer::Now();
            auto time = Timer::GetDiffSeconds(now, mModel->StartTime);
            std::cout << "Finished loading model (took " << time << " [s])\n";

            // Free model and task-related memory:
            mModel.reset(nullptr);
        }
    }
}

void AssetManager::PreprocessGltf(SceneGraphNode &root)
{
    //Load and parse gltf file:
    mModel->Gltf = std::make_unique<GltfAsset>(mModel->Config.Filepath);

    // Retrieve materials, fill table of their keys
    std::map<size_t, SceneKey> matKeyMap;
    mModel->Gltf->PreprocessMaterials(mScene, matKeyMap, mModel->ImgTasks, mModel->Config);

    // Retrieve mesh primitives, fill table of their keys, assign them materials from previous table
    mModel->Gltf->PreprocessMeshes(mScene, mModel->MeshKeyMap, mModel->PrimTasks, mModel->Config, matKeyMap);

    // Retrieve node hierarchy, assign keys from mesh table:
    mModel->Gltf->PreprocessHierarchy(root, mModel->MeshKeyMap);

    // Calculate number of async tasks to do:
    const auto primCount = mModel->ImgTasks.size();
    const auto imgCount = mModel->PrimTasks.size();

    mModel->TasksLeft = static_cast<int64_t>(primCount + imgCount);
}

void AssetManager::LoadHdri(const std::filesystem::path &path)
{
    mThreadPool->Push([this, path]() {
        if (mHDRI.LastPath != path)
        {
            mHDRI.LastPath = path;

            auto pathStr           = path.string();
            mScene.Env.HdriImage   = ImageData::ImportHDRI(pathStr.c_str());
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