#include "RenderContext.h"

#include "Renderer.h"
#include "VkInit.h"
#include <memory>

#include "HelloRenderer.h"
#include "ImGuiUtils.h"

RenderContext::RenderContext(VulkanContext &ctx)
    : mCtx(ctx), mMainDeletionQueue(ctx), mSwapchainDeletionQueue(ctx)
{
    // Create queues:
    mQueues.Graphics = vkinit::CreateQueue(ctx, vkb::QueueType::graphics);
    mQueues.Present = vkinit::CreateQueue(ctx, vkb::QueueType::present);

    // Create per frame coomand pools and sync-objects:
    for (auto &data : mFrameInfo.Data)
    {
        data.CommandPool = vkinit::CreateCommandPool(mCtx, vkb::QueueType::graphics);

        vkinit::CreateSemaphore(mCtx, data.ImageAcquiredSemaphore);
        vkinit::CreateSemaphore(mCtx, data.RenderCompletedSemaphore);
        vkinit::CreateSignalledFence(mCtx, data.InFlightFence);
    }

    for (auto &data : mFrameInfo.Data)
    {
        mMainDeletionQueue.push_back(data.CommandPool);

        mMainDeletionQueue.push_back(data.InFlightFence);
        mMainDeletionQueue.push_back(data.ImageAcquiredSemaphore);
        mMainDeletionQueue.push_back(data.RenderCompletedSemaphore);
    }

    // To-do: Move this to some factory function:
    mRenderer = std::make_unique<HelloRenderer>(mCtx, mFrameInfo, mQueues);
}

void RenderContext::InitImGuiVulkanBackend()
{
    mImGuiDescriptorPool = imutils::CreateDescriptorPool(mCtx);
    mMainDeletionQueue.push_back(mImGuiDescriptorPool);

    imutils::InitVulkanBackend(mCtx, mImGuiDescriptorPool, mQueues.Graphics, mFrameInfo.MaxInFlight);
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