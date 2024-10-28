#include "RenderContext.h"

#include "Barrier.h"
#include "Common.h"
#include "ImGuiUtils.h"
#include "Renderer.h"
#include "Utils.h"
#include "VkInit.h"

#include "HelloRenderer.h"
#include "Minimal3D.h"

#include <memory>

RenderContext::RenderContext(VulkanContext &ctx)
    : mCtx(ctx), mMainDeletionQueue(ctx), mSwapchainDeletionQueue(ctx)
{
    // Create queues:
    mQueues.Graphics = vkinit::CreateQueue(ctx, vkb::QueueType::graphics);
    mQueues.Present = vkinit::CreateQueue(ctx, vkb::QueueType::present);

    // Create per frame command pools/buffers and sync-objects:
    for (auto &data : mFrameInfo.Data)
    {
        data.CommandPool = vkinit::CreateCommandPool(mCtx, vkb::QueueType::graphics);
        data.CommandBuffer = vkinit::CreateCommandBuffer(mCtx, data.CommandPool);

        vkinit::CreateSemaphore(mCtx, data.ImageAcquiredSemaphore);
        vkinit::CreateSemaphore(mCtx, data.RenderCompletedSemaphore);
        vkinit::CreateSignalledFence(mCtx, data.InFlightFence);
    }

    // Update the deletion queue:
    for (auto &data : mFrameInfo.Data)
    {
        mMainDeletionQueue.push_back(data.CommandPool);

        mMainDeletionQueue.push_back(data.InFlightFence);
        mMainDeletionQueue.push_back(data.ImageAcquiredSemaphore);
        mMainDeletionQueue.push_back(data.RenderCompletedSemaphore);
    }

    // Create the camera
    mCamera = std::make_unique<Camera>(mCtx, mFrameInfo);

    // To-do: Move this to some factory function:
    mRenderer = std::make_unique<Minimal3DRenderer>(mCtx, mFrameInfo, mQueues, mCamera);
}

void RenderContext::InitImGuiVulkanBackend()
{
    mImGuiDescriptorPool = imutils::CreateDescriptorPool(mCtx);
    mMainDeletionQueue.push_back(mImGuiDescriptorPool);

    imutils::InitVulkanBackend(mCtx, mImGuiDescriptorPool, mQueues.Graphics,
                               mFrameInfo.MaxInFlight);
}

RenderContext::~RenderContext()
{
    mMainDeletionQueue.flush();
}

void RenderContext::OnUpdate(float deltaTime)
{
    mCamera->OnUpdate(deltaTime);
    mRenderer->OnUpdate(deltaTime);
}

void RenderContext::OnImGui()
{
    mRenderer->OnImGui();
    mCamera->OnImGui();
}

void RenderContext::OnRender()
{
    auto &frame = mFrameInfo.CurrentData();

    // 1. Wait for the in-Flight fence:
    vkWaitForFences(mCtx.Device, 1, &frame.InFlightFence, VK_TRUE, UINT64_MAX);

    // 2. Try to acquire swapchain image, bail out if that fails:
    common::AcquireNextImage(mCtx, mFrameInfo);

    if (!mCtx.SwapchainOk)
        return;

    // 3. Reset the in-Flight fence:
    vkResetFences(mCtx.Device, 1, &frame.InFlightFence);

    // 4. Draw the frame:
    DrawFrame();

    // 5. Present the frame to swapchain:
    common::PresentFrame(mCtx, mQueues, mFrameInfo);

    // 6. Advance frame index:
    mFrameInfo.Index = (mFrameInfo.Index + 1) % mFrameInfo.MaxInFlight;
}

void RenderContext::DrawFrame()
{
    auto &frame = mFrameInfo.CurrentData();
    auto &cmd = mFrameInfo.CurrentCmd();
    auto &swapchainImage = mCtx.SwapchainImages[mFrameInfo.ImageIndex];

    VkExtent2D swapchainSize{mCtx.Swapchain.extent.width, mCtx.Swapchain.extent.height};

    // I. Reset the command buffer
    vkResetCommandBuffer(cmd, 0);

    // II. Record the command buffer
    utils::BeginRecording(cmd);
    {
        // 1. Transition render target to rendering:
        barrier::ImageBarrierColorToRender(cmd, mRenderer->GetTarget());

        // 2. Render to image:
        mRenderer->OnRender();

        // 3. Transition render target and swapchain image for copy
        barrier::ImageLayoutBarrierCoarse(cmd, mRenderer->GetTarget(),
                                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        barrier::ImageLayoutBarrierCoarse(cmd, swapchainImage, VK_IMAGE_LAYOUT_UNDEFINED,
                                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        // 4. Copy render target to swapchain image
        utils::BlitImage(cmd, mRenderer->GetTarget(), swapchainImage,
                         mRenderer->GetTargetSize(), swapchainSize);

        // 5. Transition swapchain image to render
        barrier::ImageLayoutBarrierCoarse(cmd, swapchainImage,
                                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        // 6. Draw the ui on top (in native res)
        DrawUI(cmd);

        // 7. Transition swapchain image to presentation:
        barrier::ImageLayoutBarrierCoarse(cmd, swapchainImage,
                                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                          VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    }
    utils::EndRecording(cmd);

    // III. Submit the command buffer
    common::SubmitGraphicsQueue(mQueues, cmd, frame);
}

void RenderContext::DrawUI(VkCommandBuffer cmd)
{
    VkExtent2D swapchainSize{mCtx.Swapchain.extent.width, mCtx.Swapchain.extent.height};

    auto swapchainView = mCtx.SwapchainImageViews[mFrameInfo.ImageIndex];

    VkRenderingAttachmentInfoKHR colorAttachment = vkinit::CreateAttachmentInfo(
        swapchainView, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR, {});

    VkRenderingInfoKHR renderingInfo =
        vkinit::CreateRenderingInfo(swapchainSize, colorAttachment);

    vkCmdBeginRendering(cmd, &renderingInfo);
    {
        common::ViewportScissor(cmd, swapchainSize);

        imutils::RecordImguiToCommandBuffer(cmd);
    }
    vkCmdEndRendering(cmd);
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

void RenderContext::LoadScene(Scene &scene)
{
    mRenderer->LoadScene(scene);
}

void RenderContext::OnKeyPressed(int keycode, bool repeat)
{
    mCamera->OnKeyPressed(keycode, repeat);
}

void RenderContext::OnKeyReleased(int keycode)
{
    mCamera->OnKeyReleased(keycode);
}

void RenderContext::OnMouseMoved(float x, float y)
{
    mCamera->OnMouseMoved(x, y);
}