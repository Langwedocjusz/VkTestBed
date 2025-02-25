#pragma once

#include "Image.h"
#include "RenderContext.h"

#include <vulkan/vulkan.h>

class IRenderer {
  public:
    IRenderer(VulkanContext &ctx, FrameInfo &info, RenderContext::Queues &queues,
              std::unique_ptr<Camera> &camera)
        : mCtx(ctx), mFrame(info), mQueues(queues), mCamera(camera),
          mMainDeletionQueue(ctx), mSwapchainDeletionQueue(ctx),
          mPipelineDeletionQueue(ctx)
    {
    }

    virtual ~IRenderer() {};

    virtual void OnUpdate([[maybe_unused]] float deltaTime) = 0;
    virtual void OnImGui() = 0;
    virtual void OnRender() = 0;

    virtual void CreateSwapchainResources() = 0;
    virtual void RebuildPipelines() = 0;
    virtual void LoadScene(const Scene &scene) = 0;

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
    std::unique_ptr<Camera> &mCamera;

    Image mRenderTarget;
    VkImageView mRenderTargetView;

    DeletionQueue mMainDeletionQueue;
    DeletionQueue mSwapchainDeletionQueue;
    DeletionQueue mPipelineDeletionQueue;
};