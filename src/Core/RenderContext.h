#pragma once

#include "Camera.h"
#include "DeletionQueue.h"
#include "Frame.h"
#include "Scene.h"
#include "VulkanContext.h"

#include <memory>

#ifdef ENABLE_TRACY_VULKAN
#include <tracy/TracyVulkan.hpp>
#endif

class IRenderer;

class RenderContext {
  public:
    RenderContext(VulkanContext &ctx);
    ~RenderContext();

    void InitImGuiVulkanBackend();

    void OnUpdate(float deltaTime);
    void OnImGui();
    void OnRender();

    void OnKeyPressed(int keycode, bool repeat);
    void OnKeyReleased(int keycode);
    void OnMouseMoved(float x, float y);

    void CreateSwapchainResources();
    void DestroySwapchainResources();

    void LoadScene(Scene &scene);
    void RebuildPipelines();

    struct Queues {
        VkQueue Graphics;
        VkQueue Present;
    };

  private:
    void DrawFrame();
    void DrawUI(VkCommandBuffer cmd);

  private:
    VulkanContext &mCtx;

    FrameInfo mFrameInfo;
    Queues mQueues;

    std::unique_ptr<Camera> mCamera;
    std::unique_ptr<IRenderer> mRenderer;

    VkDescriptorPool mImGuiDescriptorPool;

    DeletionQueue mMainDeletionQueue;
    DeletionQueue mSwapchainDeletionQueue;

#ifdef ENABLE_TRACY_VULKAN
    std::vector<TracyVkCtx> mProfilerContexts;
#endif
};