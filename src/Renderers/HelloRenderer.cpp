#include "HelloRenderer.h"
#include "Renderer.h"
#include "Shader.h"
// #include "Vertex.h"
#include "VkInit.h"
#include <vulkan/vulkan.h>

#include "Common.h"
#include "imgui.h"

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
            .SetColorFormat(mRenderTargetFormat)
            .Build(ctx);

    mMainDeletionQueue.push_back(mGraphicsPipeline.Handle);
    mMainDeletionQueue.push_back(mGraphicsPipeline.Layout);

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

        vkCmdDraw(cmd, 3, 1, 0, 0);
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
