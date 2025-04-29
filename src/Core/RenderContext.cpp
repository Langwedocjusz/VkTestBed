#include "RenderContext.h"

#include "Barrier.h"
#include "Common.h"
#include "Frame.h"
#include "HelloRenderer.h"
#include "ImGuiInit.h"
#include "Keycodes.h"
#include "Minimal3D.h"
#include "MinimalPBR.h"
#include "Renderer.h"
#include "VkInit.h"
#include "VkUtils.h"

#include <iostream>
#include <memory>
#include <vulkan/vulkan.h>

#include "ImGuiUtils.h"

RenderContext::RenderContext(VulkanContext &ctx)
    : mCtx(ctx), mMainDeletionQueue(ctx), mSwapchainDeletionQueue(ctx)
{
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

    // Timing setup based on:
    // https://docs.vulkan.org/samples/latest/samples/api/timestamp_queries/README.html
    // Check for timestamp support:
    auto &limits = mCtx.PhysicalDevice.properties.limits;

    mTimestampPeriod = limits.timestampPeriod;
    mTimestampSupported = (mTimestampPeriod != 0.0);

    if (!limits.timestampComputeAndGraphics)
    {
        if (mCtx.QueueProperties.Graphics.timestampValidBits == 0)
        {
            mTimestampSupported = false;
        }
    }

    if (mTimestampSupported)
    {
        // Create the query pool:
        VkQueryPoolCreateInfo queryPoolInfo{};
        queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        queryPoolInfo.queryCount = static_cast<uint32_t>(mNumTimestamps);

        auto queryRes =
            vkCreateQueryPool(mCtx.Device, &queryPoolInfo, nullptr, &mQueryPool);

        if (queryRes == VK_SUCCESS)
        {
            // Reset all timestamps preemptively
            {
                auto &pool = mFrameInfo.CurrentPool();
                auto cmd = vkutils::ScopedCommand(mCtx, QueueType::Graphics, pool);

                vkCmdResetQueryPool(cmd.Buffer, mQueryPool, 0, mNumTimestamps);
            }

            mMainDeletionQueue.push_back(mQueryPool);
        }

        else
            mTimestampSupported = false;
    }

    if (!mTimestampSupported)
        std::cout << "Timestamp queries not supported!\n";

    // Create the camera
    mCamera = std::make_unique<Camera>(mCtx, mFrameInfo);

    // To-do: Move this to some factory function:
    mRenderer = std::make_unique<MinimalPbrRenderer>(mCtx, mFrameInfo, mCamera);
}

void RenderContext::InitImGuiVulkanBackend()
{
    mImGuiDescriptorPool = iminit::CreateDescriptorPool(mCtx);
    mMainDeletionQueue.push_back(mImGuiDescriptorPool);

    iminit::InitVulkanBackend(mCtx, mImGuiDescriptorPool, mFrameInfo.MaxInFlight);
}

RenderContext::~RenderContext()
{
    mMainDeletionQueue.flush();
}

void RenderContext::OnUpdate(float deltaTime)
{
    mCamera->OnUpdate(deltaTime);
    mRenderer->OnUpdate(deltaTime);

    mFrameInfo.Stats.CPUTime = 1000.0f * deltaTime;
}

void RenderContext::OnImGui()
{
    mRenderer->OnImGui();
    mCamera->OnImGui();

    if (mShowStats)
    {
        // To-do: update memory stats
        imutils::DisplayStats(mFrameInfo.Stats);
    }
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
    common::PresentFrame(mCtx, mFrameInfo);

    // 6. Advance frame index:
    mFrameInfo.FrameNumber++;
    mFrameInfo.Index = mFrameInfo.FrameNumber % mFrameInfo.MaxInFlight;
}

void RenderContext::DrawFrame()
{
    auto &frame = mFrameInfo.CurrentData();
    auto &cmd = mFrameInfo.CurrentCmd();

    auto &swapchainImage = mCtx.SwapchainImages[mFrameInfo.ImageIndex];
    auto swapchainExtent = mCtx.Swapchain.extent;

    auto swapchainInfo = vkutils::BlitImageInfo{
        .ImgHandle = swapchainImage,
        .Extent = VkExtent3D{swapchainExtent.width, swapchainExtent.height, 1},
        .NumLayers = 1,
    };

    // I. Reset the command buffer
    vkResetCommandBuffer(cmd, 0);

    // II. Record the command buffer
    vkutils::BeginRecording(cmd);
    {
        vkCmdResetQueryPool(cmd, mQueryPool, 2 * mFrameInfo.Index, 2);
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, mQueryPool,
                            2 * mFrameInfo.Index);

        // 1. Transition render target to rendering:
        barrier::ImageBarrierColorToRender(cmd, mRenderer->GetTarget().Handle);

        // 2. Render to image:
        mRenderer->OnRender();

        // 3. Transition render target and swapchain image for copy
        {
            auto info = barrier::ImageLayoutBarrierInfo{
                .Image = mRenderer->GetTarget().Handle,
                .OldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .NewLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .SubresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
            };

            barrier::ImageLayoutBarrierCoarse(cmd, info);
        }

        {
            auto info = barrier::ImageLayoutBarrierInfo{
                .Image = swapchainImage,
                .OldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .NewLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .SubresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
            };

            barrier::ImageLayoutBarrierCoarse(cmd, info);
        }

        // 4. Copy render target to swapchain image
        vkutils::BlitImageZeroMip(cmd, mRenderer->GetTarget(), swapchainInfo);

        // 5. Transition swapchain image to render
        {
            auto info = barrier::ImageLayoutBarrierInfo{
                .Image = swapchainImage,
                .OldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .NewLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .SubresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
            };

            barrier::ImageLayoutBarrierCoarse(cmd, info);
        }

        // 6. Draw the ui on top (in native res)
        DrawUI(cmd);

        // 7. Transition swapchain image to presentation:
        {
            auto info = barrier::ImageLayoutBarrierInfo{
                .Image = swapchainImage,
                .OldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .NewLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                .SubresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
            };

            barrier::ImageLayoutBarrierCoarse(cmd, info);
        }

        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, mQueryPool,
                            2 * mFrameInfo.Index + 1);
    }
    vkutils::EndRecording(cmd);

    // III. Submit the command buffer
    common::SubmitGraphicsQueue(mCtx, cmd, frame);

    // IV. Query for timestamp results (previous frame):
    auto ModDecrement = [](uint32_t x, uint32_t n) { return (x != 0) ? x - 1 : n - 1; };

    auto prevFrameIdx = ModDecrement(mFrameInfo.Index, FrameInfo::MaxInFlight);
    auto queryIdx = 2 * prevFrameIdx;

    auto queryRes = vkGetQueryPoolResults(mCtx.Device, mQueryPool,
                                          queryIdx,               // first query
                                          2,                      // query count
                                          2 * sizeof(uint64_t),   // data size
                                          &mTimestamps[queryIdx], // data
                                          sizeof(uint64_t),       // stride
                                          VK_QUERY_RESULT_64_BIT);

    if (queryRes == VK_SUCCESS)
    {
        auto diff = static_cast<float>(mTimestamps[queryIdx + 1] - mTimestamps[queryIdx]);
        mFrameInfo.Stats.GPUTime = diff * mTimestampPeriod / 1000000.0f;
    }
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

        iminit::RecordImguiToCommandBuffer(cmd);
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

void RenderContext::RebuildPipelines()
{
    mRenderer->RebuildPipelines();
}

void RenderContext::OnKeyPressed(int keycode, bool repeat)
{
    if (keycode == VKTB_KEY_KP_MULTIPLY)
        mShowStats = !mShowStats;

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