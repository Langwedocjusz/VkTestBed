#pragma once

#include "Camera.h"
#include "DeletionQueue.h"
#include "Event.h"
#include "Frame.h"
#include "RendererFactory.h"
#include "Scene.h"
#include "VulkanContext.h"
#include "VulkanStatistics.h"

#include <memory>

class RenderContext {
  public:
    RenderContext(VulkanContext &ctx, Camera &camera);
    ~RenderContext();

    /// Must be called after general imgui intiialization
    void OnInit();

    void OnUpdate(float deltaTime);
    void OnImGui();
    void OnRender(std::optional<SceneKey> highlightedObj);
    void OnEvent(Event::EventVariant event);

    void ResizeSwapchain();
    void LoadScene(Scene &scene);
    void RebuildPipelines();
    SceneKey PickObjectId(float x, float y);

  private:
    void DrawFrame(std::optional<SceneKey> highlightedObj);
    void DrawUI(VkCommandBuffer cmd);

    void CreateSwapchainResources();
    void DestroySwapchainResources();

  private:
    VulkanContext &mCtx;
    FrameInfo mFrameInfo;

    Camera &mCamera;

    RendererFactory mFactory;
    std::unique_ptr<IRenderer> mRenderer;

    // Object picking related data:
    struct {
        Texture Target;
        Texture Depth;

        Buffer ReadbackBuffer;
    } mPicking;

    bool mShowStats = false;
    VulkanStatisticsCollector mStatsCollector;

    DeletionQueue mMainDeletionQueue;
    DeletionQueue mSwapchainDeletionQueue;
};