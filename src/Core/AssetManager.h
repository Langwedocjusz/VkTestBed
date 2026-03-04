#pragma once

#include "GltfImporter.h"
#include "ModelConfig.h"
#include "Scene.h"
#include "SceneGraph.h"

class ThreadPool;

class AssetManager {
  public:
    AssetManager(Scene &scene);
    ~AssetManager();

    void OnUpdate();

    void LoadModel(const ModelConfig &config, SceneGraphNode &root, bool &isReady);
    void LoadHdri(const std::filesystem::path &path);

    void ClearCachedHDRI();

  private:
    void PreprocessGltf(SceneGraphNode &root);

  private:
    Scene &mScene;

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
        ImageTaskData                        Data;
        std::optional<std::filesystem::path> LastPath;
    } mHDRI;

    std::unique_ptr<ThreadPool> mThreadPool;
};