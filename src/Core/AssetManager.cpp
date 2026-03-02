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

struct AssetManager::Model {
    Model(const ModelConfig &config, bool &isReady)
        : Config(config), IsReady(isReady), TasksLeft(0), StartTime(Timer::Now())
    {
    }

    ModelConfig                    Config;
    bool                          &IsReady;
    fastgltf::Asset                Gltf;
    std::vector<ImageTaskData>     ImgData;
    std::vector<PrimitiveTaskData> PrimData;
    std::map<size_t, SceneKey>     MeshDict;
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

            // Mark prefab as ready:GetGltfWith
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
    mModel->Gltf = ModelLoader::GetGltfWithBuffers(mModel->Config.Filepath);
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
    using namespace std::views;

    const std::filesystem::path workingDir = mModel->Config.Filepath.parent_path();
    const std::string           baseName   = mModel->Config.Filepath.stem().string();

    // Gather ids of the mesh materials:
    std::map<size_t, SceneKey> keyMap;

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

    auto quat     = glm::quat(r.w(), r.x(), r.y(), r.z());
    auto rotation = glm::eulerAngles(quat);

    auto scale = glm::vec3(s.x(), s.y(), s.z());

    return {translation, rotation, scale};
}

void AssetManager::ProcessGltfHierarchy(SceneGraphNode &root)
{
    // TODO: Currently we assume gltf holds one scene.
    auto &scene = mModel->Gltf.scenes[0];

    for (auto nodeIdx : scene.nodeIndices)
    {
        auto &node                          = mModel->Gltf.nodes[nodeIdx];
        auto [translation, rotation, scale] = UnpackTransform(node);

        // TODO: Only handles first-level nodes that hold meshes
        if (node.meshIndex.has_value())
        {
            auto meshKey = mModel->MeshDict[*node.meshIndex];

            auto &prefabNode       = root.EmplaceChild(meshKey);
            prefabNode.Translation = translation;
            prefabNode.Rotation    = rotation;
            prefabNode.Scale       = scale;
            prefabNode.Name        = node.name;
        }
    }
}