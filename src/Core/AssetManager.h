#pragma once

#include "ModelConfig.h"
#include "Scene.h"
#include "ThreadPool.h"
#include "Timer.h"

// To-do: maybe hermetize this somehow:
#include <fastgltf/types.hpp>

#include <atomic>

class SceneEditor;

class AssetManager {
  public:
    AssetManager(Scene &scene, SceneEditor &editor);

    void OnUpdate();

    void LoadModel(const ModelConfig &config);
    void LoadHdri(const std::filesystem::path &path);

  private:
    void ParseGltf();
    void PreprocessGltfAssets();
    void ProcessGltfHierarchy();

  private:
    Scene &mScene;
    SceneEditor &mEditor;

    struct ImageTaskData {
        SceneKey ImageKey;
        std::filesystem::path Path;
        bool Unorm;
    };

    struct PrimitiveTaskData {
        SceneKey SceneMesh;
        size_t ScenePrim;
        int64_t GltfMesh;
        int64_t GltfPrim;
    };

    enum class ModelStage
    {
        Idle,
        Parsing,
        Parsed,
        Loading,
    };

    struct {
        ModelConfig Config;
        std::unique_ptr<fastgltf::Asset> Gltf;

        std::vector<ImageTaskData> ImgData;
        std::vector<PrimitiveTaskData> PrimData;

        std::map<size_t, SceneKey> MeshDict;

        ModelStage Stage = ModelStage::Idle;

        std::atomic_int64_t TasksLeft = 0;

        Timer::Point mStartTime;
    } mModel;

    struct {
        ImageTaskData Data;
    } mHDRI;

    ThreadPool mThreadPool;
};