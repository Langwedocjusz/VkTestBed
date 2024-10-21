#include "HelloRenderer.h"
#include "Renderer.h"
#include "Shader.h"
// #include "Vertex.h"
#include "Primitives.h"
#include "Utils.h"
#include "Vertex.h"
#include "VkInit.h"
#include <vulkan/vulkan.h>

#include "Common.h"
#include "imgui.h"

HelloRenderer::HelloRenderer(VulkanContext &ctx, FrameInfo &info,
                             RenderContext::Queues &queues)
    : IRenderer(ctx, info, queues)
{
    // Create Graphics Pipelines
    auto shaderStages = ShaderBuilder()
                            .SetVertexPath("assets/spirv/HelloTriangleVert.spv")
                            .SetFragmentPath("assets/spirv/HelloTriangleFrag.spv")
                            .Build(ctx);

    mGraphicsPipeline =
        PipelineBuilder()
            .SetShaderStages(shaderStages)
            .SetVertexInput<ColoredVertex>(0, VK_VERTEX_INPUT_RATE_VERTEX)
            .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetPolygonMode(VK_POLYGON_MODE_FILL)
            .SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE)
            .SetColorFormat(mRenderTargetFormat)
            .Build(ctx);

    mMainDeletionQueue.push_back(mGraphicsPipeline.Handle);
    mMainDeletionQueue.push_back(mGraphicsPipeline.Layout);

    //Create Vertex buffer:
    std::vector<ColoredVertex> vertices = HelloTriangleVertexProvider().GetVertices();

    mVertexCount = vertices.size();

    GPUBufferInfo vertInfo{
        .Usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        .Size = mVertexCount * sizeof(ColoredVertex),
        .Data = vertices.data(),
    };

    {
        auto& pool = mFrame.Data[mFrame.Index].CommandPool;
        utils::ScopedCommand cmd(mCtx, mQueues.Graphics, pool);

        mVertexBuffer = Buffer::CreateGPUBuffer(mCtx, cmd.Buffer, vertInfo);
    }

    mMainDeletionQueue.push_back(&mVertexBuffer);

    //Create swapchain resources:
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
    ImGui::ShowDemoWindow();
}

void HelloRenderer::OnRender()
{
    auto &frame = mFrame.Data[mFrame.Index];
    auto &cmd = frame.CommandBuffer;

    VkClearValue clear{{{0.0f, 0.0f, 0.0f, 0.0f}}};

    VkRenderingAttachmentInfoKHR colorAttachment = vkinit::CreateAttachmentInfo(
        mRenderTargetView, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR, clear);

    VkRenderingInfoKHR renderingInfo =
        vkinit::CreateRenderingInfo(GetTargetSize(), colorAttachment);

    vkCmdBeginRendering(cmd, &renderingInfo);
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mGraphicsPipeline.Handle);

        common::ViewportScissor(cmd, GetTargetSize());

        std::array<VkBuffer, 1> vertexBuffers{mVertexBuffer.Handle};
        std::array<VkDeviceSize, 1> offsets{0};
        vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers.data(), offsets.data());

        vkCmdDraw(cmd, mVertexCount, 1, 0, 0);
    }
    vkCmdEndRendering(cmd);
}

void HelloRenderer::CreateSwapchainResources()
{
    // Create the render target:
    auto ScaleResolution = [this](uint32_t res) {
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

    // Create the render target view:
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
