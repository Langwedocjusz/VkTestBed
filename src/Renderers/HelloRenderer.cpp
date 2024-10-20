#include "HelloRenderer.h"
#include "Renderer.h"
#include "Shader.h"
#include "Utils.h"
// #include "Vertex.h"
#include "VkInit.h"
#include <vulkan/vulkan.h>

#include "Barrier.h"
#include "Common.h"

#include <ranges>
#include <vulkan/vulkan_core.h>

HelloRenderer::HelloRenderer(VulkanContext &ctx, FrameInfo &info,
                             RenderContext::Queues &queues)
    : IRenderer(ctx, info, queues)
{
    using namespace std::views;

    // CreateGraphicsPipelines
    auto shaderStages = ShaderBuilder()
                            .SetVertexPath("assets/spirv/HelloTriangleVert.spv")
                            .SetFragmentPath("assets/spirv/HelloTriangleFrag.spv")
                            .Build(ctx);

    mGraphicsPipeline =
        PipelineBuilder()
            .SetShaderStages(shaderStages)
            //.SetVertexInput<ColoredVertex>(0, VK_VERTEX_INPUT_RATE_VERTEX)
            .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetPolygonMode(VK_POLYGON_MODE_FILL)
            .SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE)
            .SetColorFormat(mRenderTargetFormat)
            .Build(ctx);

    mMainDeletionQueue.push_back(mGraphicsPipeline.Handle);
    mMainDeletionQueue.push_back(mGraphicsPipeline.Layout);

    // Create Command Buffers:
    mCommandBuffers.resize(mFrame.MaxInFlight);

    // To-do: clang-tidy reports some error with zip instantiation here,
    // investigate it:
    for (auto [buffer, frameData] : zip(mCommandBuffers, mFrame.Data))
    {
        buffer = vkinit::CreateCommandBuffer(mCtx, frameData.CommandPool);
    }

    CreateSwapchainResources();
}

HelloRenderer::~HelloRenderer()
{
    mSwapchainDeletionQueue.flush();
    mMainDeletionQueue.flush();
}

void HelloRenderer::OnUpdate([[maybe_unused]] float deltaTime)
{
}

void HelloRenderer::OnImGui()
{
}

void HelloRenderer::OnRender()
{
    auto &frameData = mFrame.Data[mFrame.Index];

    vkWaitForFences(mCtx.Device, 1, &frameData.InFlightFence, VK_TRUE, UINT64_MAX);

    common::AcquireNextImage(mCtx, mFrame);

    if (!mCtx.SwapchainOk)
        return;

    vkResetFences(mCtx.Device, 1, &frameData.InFlightFence);

    // DrawFrame
    {
        auto &buffer = mCommandBuffers[mFrame.Index];

        vkResetCommandBuffer(buffer, 0);
        RecordCommandBuffer(buffer, mFrame.ImageIndex);

        common::SubmitGraphicsQueue(mQueues, buffer, frameData);
    }

    common::PresentFrame(mCtx, mQueues, mFrame);
}

void HelloRenderer::CreateSwapchainResources()
{
    // Create the render target:
    auto ScaleResolution = [this](uint32_t res)
    {
        return static_cast<uint32_t>(mInternalResolutionScale * static_cast<float>(res));
    };

    uint32_t width = ScaleResolution(mCtx.Swapchain.extent.width);
    uint32_t height = ScaleResolution(mCtx.Swapchain.extent.height);

    VkExtent3D drawExtent{
        .width = width,
        .height = height,
        .depth = 1,
    };

    VkImageUsageFlags drawUsage{};
    drawUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    ImageInfo renderTargetInfo{
        .Extent = drawExtent,
        .Format = mRenderTargetFormat,
        .Tiling = VK_IMAGE_TILING_OPTIMAL,
        .Usage = drawUsage,
    };

    mRenderTarget = Image::CreateImage2D(mCtx, renderTargetInfo);
    mSwapchainDeletionQueue.push_back(&mRenderTarget);

    mRenderTargetView = Image::CreateView2D(mCtx, mRenderTarget, mRenderTargetFormat,
                                            VK_IMAGE_ASPECT_COLOR_BIT);
    mSwapchainDeletionQueue.push_back(mRenderTargetView);
}

void HelloRenderer::LoadScene(const Scene &scene)
{
    // To-do: Actually unpack the scene
    // and use Vertex/Index buffers to render.

    (void)scene;
}

void HelloRenderer::RecordCommandBuffer(VkCommandBuffer commandBuffer,
                                        uint32_t imageIndex)
{
    utils::BeginRecording(commandBuffer);

    auto swapchainImage = mCtx.SwapchainImages[imageIndex];

    VkExtent2D renderSize{mRenderTarget.Info.Extent.width, mRenderTarget.Info.Extent.height};
    VkExtent2D swapchainSize{mCtx.Swapchain.extent.width, mCtx.Swapchain.extent.height};

    // 1. Transition render target to rendering:
    barrier::ImageBarrierColorToRender(commandBuffer, mRenderTarget.Handle);

    // 2. Render to image:
    VkClearValue clear{{{0.0f, 0.0f, 0.0f, 0.0f}}};

    VkRenderingAttachmentInfoKHR colorAttachment = vkinit::CreateAttachmentInfo(
        mRenderTargetView, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR, clear);

    VkRenderingInfoKHR renderingInfo =
        vkinit::CreateRenderingInfo(renderSize, colorAttachment);

    vkCmdBeginRendering(commandBuffer, &renderingInfo);
    {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          mGraphicsPipeline.Handle);

        common::ViewportScissor(commandBuffer, renderSize);

        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    }
    vkCmdEndRendering(commandBuffer);

    // 3. Transition render target and swapchain image for copy
    barrier::ImageLayoutBarrierCoarse(commandBuffer, mRenderTarget.Handle,
                                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    barrier::ImageLayoutBarrierCoarse(commandBuffer, swapchainImage,
                                      VK_IMAGE_LAYOUT_UNDEFINED,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // 4. Copy render target to swapchain image
    utils::BlitImage(commandBuffer, mRenderTarget.Handle, swapchainImage, renderSize,
                     swapchainSize);

    // 5. Transition swapchain image to presentation:
    barrier::ImageLayoutBarrierCoarse(commandBuffer, swapchainImage,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    // barrier::ImageBarrierColorToPresent(commandBuffer, swapchainImage);

    utils::EndRecording(commandBuffer);
}