#pragma once

#include "Camera.h"
#include "DeletionQueue.h"
#include "Event.h"
#include "Frame.h"
#include "RendererFactory.h"
#include "Scene.h"
#include "VulkanContext.h"

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

    bool mShowStats = false;

    bool mTimestampSupported;
    float mTimestampPeriod;

    struct Timestamp {
        uint64_t Value = 0;
        uint64_t Availability = 0;
    };

    static constexpr size_t TimestampsPerFrame = 2;

    using FrameTimestamps = std::array<Timestamp, TimestampsPerFrame>;

    std::array<FrameTimestamps, FrameInfo::MaxInFlight> mTimestamps;
    std::array<bool, FrameInfo::MaxInFlight> mTimestampFirstRun;

    // Framebuffer for object picking:
    struct {
        Texture Target;
        Texture Depth;

        Buffer ReadbackBuffer;
    } mPicking;

    DeletionQueue mMainDeletionQueue;
    DeletionQueue mSwapchainDeletionQueue;
};