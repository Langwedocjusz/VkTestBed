#pragma once

#include "Camera.h"
#include "DeletionQueue.h"
#include "Frame.h"
#include "RendererFactory.h"
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

    bool mTimestampSupported;
    float mTimestampPeriod;

    struct Timestamp {
        uint64_t Value = 0;
        uint64_t Availability = 0;
    };

    static constexpr size_t mTimestampsPerFrame = 2;
    std::vector<std::array<Timestamp, mTimestampsPerFrame>> mTimestamps;
    std::vector<bool> mTimestampFirstRun;

    DeletionQueue mMainDeletionQueue;
    DeletionQueue mSwapchainDeletionQueue;
};