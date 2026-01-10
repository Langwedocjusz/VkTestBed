#include "RenderContext.h"
#include "Pch.h"

#include "Barrier.h"
#include "Common.h"
#include "Frame.h"
#include "ImGuiInit.h"
#include "ImGuiUtils.h"
#include "ImageData.h"
#include "ImageUtils.h"
#include "Keycodes.h"
#include "Renderer.h"
#include "Scene.h"
#include "Vassert.h"
#include "VkInit.h"
#include "VkUtils.h"
#include "VulkanContext.h"

#include "volk.h"

#include <memory>

RenderContext::RenderContext(VulkanContext &ctx, Camera &camera)
    : mCtx(ctx), mCamera(camera), mFactory(ctx, mFrameInfo, mCamera),
      mStatsCollector(ctx), mMainDeletionQueue(ctx), mSwapchainDeletionQueue(ctx)
{
    // Create per frame command pools/buffers and sync-objects:
    for (auto &data : mFrameInfo.FrameData)
    {
        data.CommandPool = vkinit::CreateCommandPool(mCtx, vkb::QueueType::graphics);
        data.CommandBuffer = vkinit::CreateCommandBuffer(mCtx, data.CommandPool);

        vkinit::CreateSemaphore(mCtx, data.ImageAcquiredSemaphore);
        vkinit::CreateSignalledFence(mCtx, data.InFlightFence);
    }

    // Create per swapchain image sync-objects:
    mFrameInfo.SwapchainData.resize(mCtx.SwapchainImages.size());

    for (auto &data : mFrameInfo.SwapchainData)
    {
        vkinit::CreateSemaphore(mCtx, data.RenderCompletedSemaphore);
    }

    // Update the deletion queue:
    for (auto &data : mFrameInfo.FrameData)
    {
        mMainDeletionQueue.push_back(data.CommandPool);

        mMainDeletionQueue.push_back(data.InFlightFence);
        mMainDeletionQueue.push_back(data.ImageAcquiredSemaphore);
    }

    for (auto &data : mFrameInfo.SwapchainData)
    {
        mMainDeletionQueue.push_back(data.RenderCompletedSemaphore);
    }

    // Setup framebuffer for object picking:
    VkImageUsageFlags drawUsage{};
    drawUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    Image2DInfo renderTargetInfo{
        .Extent = VkExtent2D{1, 1},
        .Format = IRenderer::PickingTargetFormat,
        .Tiling = VK_IMAGE_TILING_OPTIMAL,
        .Usage = drawUsage,
        .MipLevels = 1,
        .Layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    };
    mPicking.Target = MakeTexture::Texture2D(mCtx, "PickingTarget", renderTargetInfo,
                                             mMainDeletionQueue);

    Image2DInfo depthBufferInfo{
        .Extent = VkExtent2D{1, 1},
        .Format = IRenderer::PickingDepthFormat,
        .Tiling = VK_IMAGE_TILING_OPTIMAL,
        .Usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .MipLevels = 1,
        .Layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
    };
    mPicking.Depth = MakeTexture::Texture2D(mCtx, "PickingDepthBuffer", depthBufferInfo,
                                            mMainDeletionQueue);

    auto bufferUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    auto bufferFlags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

    mPicking.ReadbackBuffer =
        Buffer::Create(ctx, "ReadbackBuffer", sizeof(Pixel), bufferUsage, bufferFlags);
    mMainDeletionQueue.push_back(mPicking.ReadbackBuffer);
}

void RenderContext::OnInit()
{
    iminit::InitVulkanBackend(mCtx, mFrameInfo.MaxInFlight);

    // Renderer initialized after imgui backend, since it may need
    // imgui descriptor pool to allocate descriptors
    // for debug images.
    mRenderer = mFactory.MakeRenderer(RendererType::MinimalPBR);

    CreateSwapchainResources();
}

RenderContext::~RenderContext()
{
    mMainDeletionQueue.flush();
}

void RenderContext::OnUpdate(float deltaTime)
{
    // Call renderer update:
    mRenderer->OnUpdate(deltaTime);

    // Update statistics:
    mFrameInfo.Stats.CPUTime = 1000.0f * deltaTime;

    // Retrieve info about memory usage:
    mFrameInfo.Stats.MemoryUsage = 0;
    mFrameInfo.Stats.MemoryAllocation = 0;

    std::array<VmaBudget, VK_MAX_MEMORY_HEAPS> budgets{};
    vmaGetHeapBudgets(mCtx.Allocator, &budgets[0]);

    for (uint32_t i = 0; i < VK_MAX_MEMORY_HEAPS; i++)
    {
        auto implicit = budgets[i].usage - budgets[i].statistics.blockBytes;
        auto objects = budgets[i].statistics.allocationBytes;

        mFrameInfo.Stats.MemoryUsage += implicit + objects;
        mFrameInfo.Stats.MemoryAllocation += budgets[i].usage;
    }
}

void RenderContext::OnImGui()
{
    mRenderer->OnImGui();

    if (mShowStats)
        imutils::DisplayStats(mFrameInfo.Stats);
}

void RenderContext::OnRender([[maybe_unused]] std::optional<SceneKey> highlightedObj)
{
    auto &frameData = mFrameInfo.CurrentFrameData();

    // 1. Wait for the in-Flight fence:
    vkWaitForFences(mCtx.Device, 1, &frameData.InFlightFence, VK_TRUE, UINT64_MAX);

    // 2. Try to acquire swapchain image, bail out if that fails:
    if (mCtx.SwapchainOk)
    {
        VkResult result = vkAcquireNextImageKHR(mCtx.Device, mCtx.Swapchain, UINT64_MAX,
                                                frameData.ImageAcquiredSemaphore,
                                                VK_NULL_HANDLE, &mFrameInfo.ImageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            mCtx.SwapchainOk = false;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            vpanic("Failed to acquire swapchain image!");
        }
    }

    if (!mCtx.SwapchainOk)
        return;

    // 3. Reset the in-Flight fence:
    vkResetFences(mCtx.Device, 1, &frameData.InFlightFence);

    // 4. Draw the frame:
    DrawFrame(highlightedObj);

    // 5. Present the frame to swapchain:
    auto &swapchainData = mFrameInfo.CurrentSwapchainData();

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &swapchainData.RenderCompletedSemaphore;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &mCtx.Swapchain.swapchain;
    present_info.pImageIndices = &mFrameInfo.ImageIndex;

    VkResult result = vkQueuePresentKHR(mCtx.Queues.Present, &present_info);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        mCtx.SwapchainOk = false;
        return;
    }
    else if (result != VK_SUCCESS)
    {
        vpanic("Failed to present swapchain image!");
    }

    // 6. Update frame number, advance frame index:
    mFrameInfo.FrameNumber++;
    mFrameInfo.Index = (mFrameInfo.Index + 1) % mFrameInfo.MaxInFlight;
}

void RenderContext::DrawFrame(std::optional<SceneKey> highlightedObj)
{
    auto &frame = mFrameInfo.CurrentFrameData();
    auto &swap = mFrameInfo.CurrentSwapchainData();
    auto &cmd = mFrameInfo.CurrentCmd();

    auto &swapchainImage = mCtx.SwapchainImages[mFrameInfo.ImageIndex];

    auto queryResult = mStatsCollector.QueryResults(mFrameInfo.Index);

    if (auto timeMS = queryResult.FrameTimeMS)
        mFrameInfo.Stats.GPUTime = *timeMS;

    if (auto fragCount = queryResult.FragmentInvocations)
    {
        auto targetSize = mRenderer->GetTargetSize();
        auto targetPixels = targetSize.width * targetSize.height;

        mFrameInfo.Stats.FragmentInvocations = *fragCount;
        mFrameInfo.Stats.FragmentPercent =
            100.0f * static_cast<float>(*fragCount) / static_cast<float>(targetPixels);
    }

    // I. Reset the command buffer
    vkResetCommandBuffer(cmd, 0);

    // II. Record the command buffer
    vkutils::BeginRecording(cmd);
    {
        mStatsCollector.TimestampTop(cmd, mFrameInfo.Index);

        // 1. Transition render target to rendering:
        // TODO: Maybe renderer should handle it by itself to make it more flexible
        barrier::ImageBarrierColorToRender(cmd, mRenderer->GetTargetImage().Handle);

        // 2. Render to image:
        mStatsCollector.PipelineStatsStart(cmd, mFrameInfo.Index);
        mRenderer->OnRender(highlightedObj);
        mStatsCollector.PipelineStatsEnd(cmd, mFrameInfo.Index);

        // 3. Transition render target and swapchain image for copy:
        barrier::ImageBarrierColorToTransfer(cmd, mRenderer->GetTargetImage().Handle);
        barrier::ImageBarrierSwapchainToTransfer(cmd, swapchainImage);

        // 4. Copy render target to swapchain image
        auto &swapExt = mCtx.Swapchain.extent;

        auto swapchainInfo = vkutils::BlitImageInfo{
            .ImgHandle = swapchainImage,
            .Extent = VkExtent3D{swapExt.width, swapExt.height, 1},
            .NumLayers = 1,
        };

        vkutils::BlitImageZeroMip(cmd, mRenderer->GetTargetImage(), swapchainInfo);

        // 5. Transition swapchain image to render:
        barrier::ImageBarrierSwapchainToRender(cmd, swapchainImage);

        // 6. Draw the ui on top (in native res)
        DrawUI(cmd);

        // 7. Transition swapchain image to presentation:
        barrier::ImageBarrierSwapchainToPresent(cmd, swapchainImage);

        mStatsCollector.TimestampBottom(cmd, mFrameInfo.Index);
    }
    vkutils::EndRecording(cmd);

    // III. Submit the command buffer
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    common::SubmitQueue(mCtx.Queues.Graphics, cmd, frame.InFlightFence,
                        frame.ImageAcquiredSemaphore, waitStage,
                        swap.RenderCompletedSemaphore);
}

void RenderContext::DrawUI(VkCommandBuffer cmd)
{
    VkExtent2D swapchainSize{mCtx.Swapchain.extent.width, mCtx.Swapchain.extent.height};

    auto swapchainView = mCtx.SwapchainImageViews[mFrameInfo.ImageIndex];

    common::BeginRenderingColor(cmd, swapchainSize, swapchainView, false);
    {
        common::ViewportScissor(cmd, swapchainSize);

        iminit::RecordImguiToCommandBuffer(cmd);
    }
    vkCmdEndRendering(cmd);
}

void RenderContext::CreateSwapchainResources()
{
    // Transition all swapchain images to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
    mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
        for (auto &img : mCtx.SwapchainImages)
        {
            auto barrierInfo = barrier::ImageLayoutBarrierInfo{
                .Image = img,
                .OldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .NewLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                .SubresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
            };
            barrier::ImageLayoutBarrierCoarse(cmd, barrierInfo);
        }
    });

    mRenderer->RecreateSwapchainResources();
}

void RenderContext::DestroySwapchainResources()
{
    mRenderer->DestroySwapchainResources();
    mSwapchainDeletionQueue.flush();
}

void RenderContext::ResizeSwapchain()
{
    vkDeviceWaitIdle(mCtx.Device);

    DestroySwapchainResources();
    mCtx.CreateSwapchain();
    CreateSwapchainResources();

    mCtx.SwapchainOk = true;
}

void RenderContext::LoadScene(Scene &scene)
{
    vkDeviceWaitIdle(mCtx.Device);

    mRenderer->LoadScene(scene);
    scene.ClearUpdateFlags();
}

void RenderContext::RebuildPipelines()
{
    vkDeviceWaitIdle(mCtx.Device);

    mRenderer->RebuildPipelines();
}

void RenderContext::OnEvent(Event::EventVariant event)
{
    if (std::holds_alternative<Event::Key>(event))
    {
        auto e = std::get<Event::Key>(event);

        if (e.Keycode == VKTB_KEY_KP_MULTIPLY && e.Action == VKTB_PRESS)
            mShowStats = !mShowStats;
    }
}

SceneKey RenderContext::PickObjectId(float x, float y)
{
    vkDeviceWaitIdle(mCtx.Device);

    mCtx.ImmediateSubmitGraphics([&, x, y](VkCommandBuffer cmd) {
        // Transitions layouts:
        auto info = barrier::ImageLayoutBarrierInfo{
            .Image = mPicking.Target.Img.Handle,
            .OldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .NewLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .SubresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        };
        barrier::ImageLayoutBarrierCoarse(cmd, info);

        // Draw objects:
        common::BeginRenderingColorDepth(cmd, VkExtent2D{1, 1}, mPicking.Target.View,
                                         mPicking.Depth.View, false, true, true);
        mRenderer->RenderObjectId(cmd, x, y);
        vkCmdEndRendering(cmd);

        // Copy from target image to cpu visible buffer:
        info.OldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        info.NewLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier::ImageLayoutBarrierCoarse(cmd, info);

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = mPicking.Target.Img.Info.extent;

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;

        vkCmdCopyImageToBuffer(cmd, mPicking.Target.Img.Handle,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               mPicking.ReadbackBuffer.Handle, 1, &region);
    });

    // Unpack copied data after submission:
    const auto pixel =
        static_cast<const Pixel *>(mPicking.ReadbackBuffer.AllocInfo.pMappedData);
    uint32_t R = pixel->R, G = pixel->G, B = pixel->B, A = pixel->A;

    uint32_t res = 0;
    res = res | R << 0;
    res = res | G << 8;
    res = res | B << 16;
    res = res | A << 24;

    // printf("Got ID: %u (from R: %u G: %u B: %u A: %u) \n", res, R, G, B, A);

    return static_cast<SceneKey>(res);
}