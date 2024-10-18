#pragma once

#include "DeletionQueue.h"
#include "Scene.h"
#include "VulkanContext.h"

#include <array>

struct FrameData {
    VkFence InFlightFence;
    VkSemaphore ImageAcquiredSemaphore;
    VkSemaphore RenderCompletedSemaphore;
};

struct FrameInfo {
    static constexpr size_t MaxInFlight = 2;

    std::array<FrameData, MaxInFlight> Data;

    size_t Index = 0;
    uint32_t ImageIndex = 0;
};

class IRenderer;

class RenderContext {
  public:
    RenderContext(VulkanContext &ctx);
    ~RenderContext();

    void OnUpdate(float deltaTime);
    void OnImGui();
    void OnRender();

    void CreateSwapchainResources();
    void DestroySwapchainResources();

    void LoadScene(const Scene &scene);

    struct Queues {
        VkQueue Graphics;
        VkQueue Present;
    };

  private:
    VulkanContext &mCtx;

    std::unique_ptr<IRenderer> mRenderer;

    FrameInfo mFrameInfo;
    Queues mQueues;

    DeletionQueue mMainDeletionQueue;
    DeletionQueue mSwapchainDeletionQueue;
};