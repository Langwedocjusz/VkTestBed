#pragma once

#include "Camera.h"
#include "RendererFactory.h"
#include "DeletionQueue.h"
#include "Frame.h"
#include "Scene.h"
#include "VulkanContext.h"

#include <memory>

class RenderContext {
  public:
    RenderContext(VulkanContext &ctx);
    ~RenderContext();

    /// Must be called after general imgui intiialization
    void OnInit();

    void OnUpdate(float deltaTime);
    void OnImGui();
    void OnRender();

    void OnKeyPressed(int keycode, bool repeat);
    void OnKeyReleased(int keycode);
    void OnMouseMoved(float x, float y);

    void ResizeSwapchan();
    void LoadScene(Scene &scene);
    void RebuildPipelines();

  private:
    void DrawFrame();
    void DrawUI(VkCommandBuffer cmd);

    void CreateSwapchainResources();
    void DestroySwapchainResources();

  private:
    VulkanContext &mCtx;
    FrameInfo mFrameInfo;

    Camera mCamera;

    RendererFactory mFactory;
    std::unique_ptr<IRenderer> mRenderer;

    VkDescriptorPool mImGuiDescriptorPool;

    bool mShowStats = false;

    bool mTimestampSupported = true;
    float mTimestampPeriod;
    std::vector<uint64_t> mTimestamps;
    uint32_t mPrevImgIdx;

    VkQueryPool mQueryPool;

    DeletionQueue mMainDeletionQueue;
    DeletionQueue mSwapchainDeletionQueue;
};