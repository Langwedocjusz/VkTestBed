#include "HelloRenderer.h"
#include "Renderer.h"
#include "Shader.h"
#include "Utils.h"
// #include "Vertex.h"
#include "VkInit.h"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include "Barrier.h"
#include "Common.h"

HelloRenderer::HelloRenderer(VulkanContext &ctx, FrameInfo &info,
                             RenderContext::Queues &queues)
    : IRenderer(ctx, info, queues)
{
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
            .SetColorFormat(ctx.Swapchain.image_format)
            .Build(ctx);

    mMainDeletionQueue.push_back(mGraphicsPipeline.Handle);
    mMainDeletionQueue.push_back(mGraphicsPipeline.Layout);

    // Create Command Pool:
    mCommandPool = vkinit::CreateCommandPool(mCtx, vkb::QueueType::graphics);
    mMainDeletionQueue.push_back(mCommandPool);

    // Create Command Buffers:
    mCommandBuffers.resize(mFrame.MaxInFlight);
    vkinit::AllocateCommandBuffers(ctx, mCommandBuffers, mCommandPool);
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

        auto buffers = std::array<VkCommandBuffer, 1>{buffer};

        common::SubmitGraphicsQueueDefault(mQueues.Graphics, buffers, frameData);
    }

    common::PresentFrame(mCtx, mQueues.Present, mFrame);
}

void HelloRenderer::CreateSwapchainResources()
{
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
    auto image = mCtx.SwapchainImages[imageIndex];
    auto imageView = mCtx.SwapchainImageViews[imageIndex];

    utils::BeginRecording(commandBuffer);

    barrier::ImageBarrierColorToRender(commandBuffer, image);

    VkClearValue clear{{{0.0f, 0.0f, 0.0f, 0.0f}}};

    VkRenderingAttachmentInfoKHR colorAttachment = vkinit::CreateAttachmentInfo(
        imageView, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR, clear);

    VkRenderingInfoKHR renderingInfo =
        vkinit::CreateRenderingInfo(mCtx.Swapchain.extent, colorAttachment);

    vkCmdBeginRendering(commandBuffer, &renderingInfo);
    {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          mGraphicsPipeline.Handle);

        common::ViewportScissorDefaultBehaviour(mCtx, commandBuffer);

        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    }
    vkCmdEndRendering(commandBuffer);

    barrier::ImageBarrierColorToPresent(commandBuffer, image);

    utils::EndRecording(commandBuffer);
}