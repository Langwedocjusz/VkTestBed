#pragma once

#include "ModelConfig.h"
#include "Scene.h"
#include "ThreadPool.h"

// To-do: maybe hermetize this somehow:
#include <fastgltf/types.hpp>

#include <atomic>
#include <chrono>

class AssetManager {
  public:
    AssetManager(Scene &scene);

    void OnUpdate();

    void LoadModel(const ModelConfig &config);
    void LoadHdri(const std::filesystem::path &path);

  private:
    void ParseGltf();
    void EmplaceThings();
    void ScheduleLoading();

  private:
    Scene &mScene;

    struct ImageTaskData {
        SceneKey ImageKey;
        std::filesystem::path Path;
        bool Unorm;
    };

    struct PrimitiveTaskData {
        SceneKey TgtMesh;
        size_t TgtPrimitive;
        int64_t SrcMesh;
        int64_t SrcPrimitive;
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

        ModelStage Stage = ModelStage::Idle;

        std::atomic_int64_t TasksLeft = 0;

        std::chrono::time_point<std::chrono::high_resolution_clock> mStartTime;
    } mModel;

    struct {
        ImageTaskData Data;
    } mHDRI;

    std::mutex mSceneMutex;

    ThreadPool mThreadPool;
};