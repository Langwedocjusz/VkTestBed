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
    // Path is assumed to be utf8 encoded:
    void LoadHdri(const std::string &path);

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

    // Path assumed to be utf8 encoded:
    struct {
        ImageTaskData              Data;
        std::optional<std::string> LastPath;
    } mHDRI;

    std::unique_ptr<ThreadPool> mThreadPool;
};