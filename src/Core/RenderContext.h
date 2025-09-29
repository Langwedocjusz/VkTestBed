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
    void OnRender(std::optional<SceneKey> highlightedObj);

    void OnKeyPressed(int keycode, bool repeat);
    void OnKeyReleased(int keycode);
    void OnMouseMoved(float x, float y);

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

    // Framebuffer for object picking:
    struct {
        Image Target;
        VkImageView TargetView;
        Image Depth;
        VkImageView DepthView;

        Buffer ReadbackBuffer;
    } mPicking;

    DeletionQueue mMainDeletionQueue;
    DeletionQueue mSwapchainDeletionQueue;
};