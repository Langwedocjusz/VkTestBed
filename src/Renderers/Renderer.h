#pragma once

#include "RenderContext.h"

#include "Image.h"
#include <vulkan/vulkan.h>

class IRenderer {
  public:
    IRenderer(VulkanContext &ctx, FrameInfo &info, RenderContext::Queues &queues,
              Camera &camera)
        : mCtx(ctx), mFrame(info), mQueues(queues), mCamera(camera),
          mMainDeletionQueue(ctx), mSwapchainDeletionQueue(ctx), mSceneDeletionQueue(ctx)
    {
    }

    virtual ~IRenderer() {};

    virtual void OnUpdate([[maybe_unused]] float deltaTime) = 0;
    virtual void OnImGui() = 0;
    virtual void OnRender() = 0;

    virtual void CreateSwapchainResources() = 0;

    // To-do:
    // virtual void RebuildPipelines() = 0;

    virtual void LoadScene(Scene &scene) = 0;

  public:
    void DestroySwapchainResources()
    {
        mSwapchainDeletionQueue.flush();
    }

    [[nodiscard]] VkImage GetTarget() const
    {
        return mRenderTarget.Handle;
    }
    [[nodiscard]] VkImageView GetTargetView() const
    {
        return mRenderTargetView;
    }
    [[nodiscard]] VkExtent2D GetTargetSize() const
    {
        return {mRenderTarget.Info.Extent.width, mRenderTarget.Info.Extent.height};
    }

  protected:
    VulkanContext &mCtx;
    FrameInfo &mFrame;
    RenderContext::Queues &mQueues;
    Camera &mCamera;

    Image mRenderTarget;
    VkImageView mRenderTargetView;

    DeletionQueue mMainDeletionQueue;
    DeletionQueue mSwapchainDeletionQueue;
    DeletionQueue mSceneDeletionQueue;
};