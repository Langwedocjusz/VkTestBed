#pragma once

#include "Camera.h"
#include "DeletionQueue.h"
#include "Frame.h"
#include "Scene.h"
#include "VulkanContext.h"

#include <memory>

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

    FrameInfo mFrameInfo;
    Queues mQueues;

    std::unique_ptr<Camera> mCamera;
    std::unique_ptr<IRenderer> mRenderer;

    VkDescriptorPool mImGuiDescriptorPool;

    DeletionQueue mMainDeletionQueue;
    DeletionQueue mSwapchainDeletionQueue;
};