#pragma once

#include "RenderContext.h"

class IRenderer {
  public:
    IRenderer(VulkanContext &ctx, FrameInfo &info, RenderContext::Queues &queues)
        : mCtx(ctx), mFrame(info), mQueues(queues), mMainDeletionQueue(ctx),
          mSwapchainDeletionQueue(ctx)
    {
    }

    virtual ~IRenderer() {};

    virtual void OnUpdate([[maybe_unused]] float deltaTime) = 0;
    virtual void OnImGui() = 0;
    virtual void OnRender() = 0;

    virtual void CreateSwapchainResources() = 0;

    void DestroySwapchainResources()
    {
        mSwapchainDeletionQueue.flush();
    }

    // To-do:
    // virtual void RebuildPipelines() = 0;

    virtual void LoadScene(const Scene &scene) = 0;

  protected:
    VulkanContext &mCtx;
    FrameInfo &mFrame;
    RenderContext::Queues &mQueues;

    DeletionQueue mMainDeletionQueue;
    DeletionQueue mSwapchainDeletionQueue;
};