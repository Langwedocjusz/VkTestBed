#include "HelloRenderer.h"
#include "Renderer.h"
#include "Shader.h"
#include "Vertex.h"
#include "VkInit.h"
#include <vulkan/vulkan.h>

#include "Common.h"

HelloRenderer::HelloRenderer(VulkanContext &ctx, FrameInfo &info, RenderContext::Queues& queues)
    : IRenderer(ctx, info, queues)
{
    //CreateGraphicsPipelines
    auto shaderStages = ShaderBuilder()
                            .SetVertexPath("assets/spirv/HelloTriangleVert.spv")
                            .SetFragmentPath("assets/spirv/HelloTriangleFrag.spv")
                            .Build(ctx);

    //auto bindingDescription =
    //    GetBindingDescription<ColoredVertex>(0, VK_VERTEX_INPUT_RATE_VERTEX);

    //auto attributeDescriptions = ColoredVertex::GetAttributeDescriptions();

    mGraphicsPipeline = PipelineBuilder()
                            .SetShaderStages(shaderStages)
                            //.SetVertexInput(bindingDescription, attributeDescriptions)
                            .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                            .SetPolygonMode(VK_POLYGON_MODE_FILL)
                            .SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE)
                            .DisableDepthTest()
                            .SetColorFormat(ctx.Swapchain.image_format)
                            .Build(ctx);

    mMainDeletionQueue.push_back([&]() {
        vkDestroyPipeline(ctx.Device, mGraphicsPipeline.Handle, nullptr);
        vkDestroyPipelineLayout(ctx.Device, mGraphicsPipeline.Layout, nullptr);
    });

    //Create Command Pool:
    mCommandPool = vkinit::CreateCommandPool(mCtx, vkb::QueueType::graphics);

    mMainDeletionQueue.push_back(
        [&]() { vkDestroyCommandPool(ctx.Device, mCommandPool, nullptr); });

    //Create Command Buffers:
    mCommandBuffers.resize(mFrame.MaxInFlight);
    vkinit::AllocateCommandBuffers(ctx, mCommandBuffers, mCommandPool);
}

HelloRenderer::~HelloRenderer()
{
    mSwapchainDeletionQueue.flush();
    mMainDeletionQueue.flush();
}

void HelloRenderer::OnUpdate([[maybe_unused]] float deltaTime)
{}

void HelloRenderer::OnImGui()
{}

void HelloRenderer::OnRender()
{
    auto& frameData = mFrame.Data[mFrame.Index];

    vkWaitForFences(mCtx.Device, 1, &frameData.InFlightFence, VK_TRUE, UINT64_MAX);

    common::AcquireNextImage(mCtx, frameData.ImageAcquiredSemaphore, mFrame.ImageIndex);

    if (!mCtx.SwapchainOk)
        return;

    vkResetFences(mCtx.Device, 1, &frameData.InFlightFence);

    // DrawFrame
    {
        auto &buffer = mCommandBuffers[mFrame.Index];

        vkResetCommandBuffer(buffer, 0);
        RecordCommandBuffer(buffer, mFrame.ImageIndex);

        auto buffers = std::array<VkCommandBuffer, 1>{buffer};

        common::SubmitGraphicsQueueDefault(mQueues.Graphics, buffers, frameData.InFlightFence,
                                           frameData.ImageAcquiredSemaphore,
                                           frameData.RenderCompletedSemaphore);
    }

    common::PresentFrame(mCtx, mQueues.Present, frameData.RenderCompletedSemaphore, mFrame.ImageIndex);
}

void HelloRenderer::CreateSwapchainResources()
{}

void HelloRenderer::LoadScene(const Scene &scene)
{
    //To-do: Actually unpack the scene
    //and use Vertex/Index buffers to render.

    (void)scene;
}

void HelloRenderer::RecordCommandBuffer(VkCommandBuffer commandBuffer,
                                                uint32_t imageIndex)
{
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &begin_info) != VK_SUCCESS)
        throw std::runtime_error("Failed to begin recording command buffer!");

    common::ImageBarrierColorToRender(commandBuffer, mCtx.SwapchainImages[imageIndex]);

    VkRenderingAttachmentInfoKHR colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    colorAttachment.imageView = mCtx.SwapchainImageViews[imageIndex];
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue.color = {{0.0f, 0.0f, 0.0f, 0.0f}};

    VkRenderingInfoKHR renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    renderingInfo.renderArea = {
        {0, 0}, {mCtx.Swapchain.extent.width, mCtx.Swapchain.extent.height}};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;

    vkCmdBeginRendering(commandBuffer, &renderingInfo);
    {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          mGraphicsPipeline.Handle);

        common::ViewportScissorDefaultBehaviour(mCtx, commandBuffer);

        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    }
    vkCmdEndRendering(commandBuffer);

    common::ImageBarrierColorToPresent(commandBuffer, mCtx.SwapchainImages[imageIndex]);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to record command buffer!");
}