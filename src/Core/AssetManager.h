#pragma once

#include "ModelConfig.h"
#include "Scene.h"
#include "SceneGraph.h"
#include "ThreadPool.h"

#include <unordered_map>

class AssetManager {
  public:
    AssetManager(Scene &scene);
    ~AssetManager();

    void OnUpdate();

    void LoadModel(const ModelConfig &config, SceneGraphNode &root);
    void LoadHdri(const std::filesystem::path &path);

  private:
    void ParseGltf();
    void PreprocessGltfAssets();
    void ProcessGltfHierarchy(SceneGraphNode &root);

  private:
    Scene &mScene;

    struct ImageTaskData {
        SceneKey ImageKey;
        std::optional<std::filesystem::path> Path;
        Pixel BaseColor;
        std::string Name;
        bool Unorm;
    };

    struct PrimitiveTaskData {
        SceneKey SceneMesh;
        size_t ScenePrim;
        int64_t GltfMesh;
        int64_t GltfPrim;
    };

    struct Model;
    std::unique_ptr<Model> mModel;

    enum class ModelStage
    {
        Idle,
        Parsing,
        Parsed,
        Loading,
    };

    ModelStage mModelStage = ModelStage::Idle;

    struct {
        ImageTaskData Data;
    } mHDRI;

    // Doesn't cache gltf textures loaded with a gltf,
    // as this will be taken care of by (whole) gltf caching.
    // Trying to cache at higher granularity would be
    // inconsistent anyway as some gltf materials lack a path
    //(single pixel ones).
    std::unordered_map<std::filesystem::path, SceneKey> mImagePathCache;

    ThreadPool mThreadPool;
};