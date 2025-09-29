#pragma once

#include "Camera.h"
#include "DeletionQueue.h"
#include "Frame.h"
#include "Image.h"
#include "Scene.h"

#include <vulkan/vulkan.h>

class IRenderer {
  public:
    IRenderer(VulkanContext &ctx, FrameInfo &info, Camera &camera)
        : mCtx(ctx), mFrame(info), mCamera(camera), mMainDeletionQueue(ctx),
          mSwapchainDeletionQueue(ctx), mPipelineDeletionQueue(ctx)
    {
    }

    virtual ~IRenderer() {};

    virtual void OnUpdate([[maybe_unused]] float deltaTime) = 0;
    virtual void OnImGui() = 0;
    virtual void OnRender([[maybe_unused]] std::optional<SceneKey> highlightedObj) = 0;

    virtual void CreateSwapchainResources() = 0;
    virtual void RebuildPipelines() = 0;
    virtual void LoadScene(const Scene &scene) = 0;
    virtual void RenderObjectId(VkCommandBuffer cmd, float x, float y) = 0;

    static constexpr VkFormat PickingTargetFormat = VK_FORMAT_R8G8B8A8_UINT;
    static constexpr VkFormat PickingDepthFormat = VK_FORMAT_D32_SFLOAT;

  public:
    void DestroySwapchainResources()
    {
        mSwapchainDeletionQueue.flush();
    }

    [[nodiscard]] Image &GetTarget()
    {
        return mRenderTarget;
    }
    [[nodiscard]] VkImageView GetTargetView() const
    {
        return mRenderTargetView;
    }
    [[nodiscard]] VkExtent2D GetTargetSize() const
    {
        return {mRenderTarget.Info.extent.width, mRenderTarget.Info.extent.height};
    }

  protected:
    VulkanContext &mCtx;
    FrameInfo &mFrame;
    Camera &mCamera;

    Image mRenderTarget;
    VkImageView mRenderTargetView;

    DeletionQueue mMainDeletionQueue;
    DeletionQueue mSwapchainDeletionQueue;
    DeletionQueue mPipelineDeletionQueue;
};