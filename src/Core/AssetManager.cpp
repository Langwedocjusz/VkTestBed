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
#include <ranges>
#include <variant>

struct AssetManager::Model {
    Model(const ModelConfig &config, bool &isReady)
        : Config(config), IsReady(isReady), TasksLeft(0), StartTime(Timer::Now())
    {
    }

    ModelConfig                    Config;
    bool                          &IsReady;
    std::unique_ptr<GltfAsset>     Gltf;
    std::vector<ImageTaskData>     ImgData;
    std::vector<PrimitiveTaskData> PrimData;
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
                    img          = ImageData::ImportImage(pathStr.c_str(), data.Unorm);
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

                auto &srcMesh      = mModel->Gltf.meshes[data.GltfMesh];
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
            auto now  = Timer::Now();
            auto time = Timer::GetDiffSeconds(now, mModel->StartTime);
            std::cout << "Finished loading model (took " << time << " [s])\n";

            // Free task-related memory:
            mModel.reset(nullptr);
        }
    }
}

void AssetManager::ParseGltf()
{
    mModel->Gltf = std::make_unique<GltfAsset>(mModel->Config.Filepath);
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

void AssetManager::PreprocessGltfAssets()
{
    

    // Set the number of tasks to do:
    mModel->TasksLeft =
        static_cast<int64_t>(mModel->ImgData.size() + mModel->PrimData.size());
}



void AssetManager::ProcessGltfHierarchy(SceneGraphNode &root)
{
    
}