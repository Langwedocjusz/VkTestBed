#pragma once

#include "Image.h"
#include "Frame.h"
#include "Camera.h"
#include "Scene.h"

#include <vulkan/vulkan.h>

#include <memory>

class IRenderer {
  public:
    IRenderer(VulkanContext &ctx, FrameInfo &info,
              std::unique_ptr<Camera> &camera)
        : mCtx(ctx), mFrame(info), mCamera(camera),
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

    [[nodiscard]] const Image &GetTarget() const
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
    std::unique_ptr<Camera> &mCamera;

    Image mRenderTarget;
    VkImageView mRenderTargetView;

    DeletionQueue mMainDeletionQueue;
    DeletionQueue mSwapchainDeletionQueue;
    DeletionQueue mPipelineDeletionQueue;
};