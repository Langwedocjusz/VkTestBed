#pragma once

#include "DeletionQueue.h"
#include "Scene.h"
#include "VulkanContext.h"

#include <array>
#include <memory>

struct FrameData {
    VkFence InFlightFence;
    VkSemaphore ImageAcquiredSemaphore;
    VkSemaphore RenderCompletedSemaphore;

    VkCommandPool CommandPool;
    VkCommandBuffer CommandBuffer;
};

struct FrameInfo {
    static constexpr size_t MaxInFlight = 2;

    std::array<FrameData, MaxInFlight> Data;

    size_t Index = 0;
    uint32_t ImageIndex = 0;

    auto &CurrentData()
    {
        return Data[Index];
    }
    auto &CurrentCmd()
    {
        return Data[Index].CommandBuffer;
    }
    auto &CurrentPool()
    {
        return Data[Index].CommandPool;
    }
};

class IRenderer;

class RenderContext {
  public:
    RenderContext(VulkanContext &ctx);
    ~RenderContext();

    void InitImGuiVulkanBackend();

    void OnUpdate(float deltaTime);
    void OnImGui();
    void OnRender();

    void CreateSwapchainResources();
    void DestroySwapchainResources();

    void LoadScene(Scene &scene);

    struct Queues {
        VkQueue Graphics;
        VkQueue Present;
    };

  private:
    void DrawFrame();
    void DrawUI(VkCommandBuffer cmd);

  private:
    VulkanContext &mCtx;

    std::unique_ptr<IRenderer> mRenderer;

    FrameInfo mFrameInfo;
    Queues mQueues;

    VkDescriptorPool mImGuiDescriptorPool;

    DeletionQueue mMainDeletionQueue;
    DeletionQueue mSwapchainDeletionQueue;
};