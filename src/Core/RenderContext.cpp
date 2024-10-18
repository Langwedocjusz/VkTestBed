#include "RenderContext.h"

#include "Renderer.h"
#include "VkInit.h"
#include <memory>

#include "HelloRenderer.h"

RenderContext::RenderContext(VulkanContext &ctx) : mCtx(ctx)
{
    // Create queues:
    mQueues.Graphics = vkinit::CreateQueue(ctx, vkb::QueueType::graphics);
    mQueues.Present = vkinit::CreateQueue(ctx, vkb::QueueType::present);

    // Create per frame sync-objects:
    for (auto &frameData : mFrameInfo.Data)
    {
        vkinit::CreateSemaphore(mCtx, frameData.ImageAcquiredSemaphore);
        vkinit::CreateSemaphore(mCtx, frameData.RenderCompletedSemaphore);
        vkinit::CreateSignalledFence(mCtx, frameData.InFlightFence);
    }

    mMainDeletionQueue.push_back([&]() {
        for (size_t i = 0; i < mFrameInfo.MaxInFlight; i++)
        {
            auto &data = mFrameInfo.Data[i];

            vkDestroyFence(ctx.Device, data.InFlightFence, nullptr);
            vkDestroySemaphore(ctx.Device, data.ImageAcquiredSemaphore, nullptr);
            vkDestroySemaphore(ctx.Device, data.RenderCompletedSemaphore, nullptr);
        }
    });

    //To-do: Move this to some factory function:
    mRenderer = std::make_unique<HelloRenderer>(mCtx, mFrameInfo, mQueues);
}

RenderContext::~RenderContext()
{
    mMainDeletionQueue.flush();
}

void RenderContext::OnUpdate(float deltaTime)
{
    mRenderer->OnUpdate(deltaTime);
}

void RenderContext::OnImGui()
{
    mRenderer->OnImGui();
}

void RenderContext::OnRender()
{
    mRenderer->OnRender();

    mFrameInfo.Index = (mFrameInfo.Index + 1) % mFrameInfo.MaxInFlight;
}

void RenderContext::CreateSwapchainResources()
{
    mRenderer->CreateSwapchainResources();
}

void RenderContext::DestroySwapchainResources()
{
    mRenderer->DestroySwapchainResources();
    mSwapchainDeletionQueue.flush();
}

void RenderContext::LoadScene(const Scene &scene)
{
    mRenderer->LoadScene(scene);
}