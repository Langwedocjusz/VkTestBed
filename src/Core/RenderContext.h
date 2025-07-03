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

    std::unique_ptr<Camera> mCamera;
    std::unique_ptr<IRenderer> mRenderer;

    VkDescriptorPool mImGuiDescriptorPool;

    bool mShowStats = false;

    bool mTimestampSupported = true;
    float mTimestampPeriod;

    static const size_t mNumTimestamps = 2 * FrameInfo::MaxInFlight;
    std::array<uint64_t, mNumTimestamps> mTimestamps;

    VkQueryPool mQueryPool;

    DeletionQueue mMainDeletionQueue;
    DeletionQueue mSwapchainDeletionQueue;
};