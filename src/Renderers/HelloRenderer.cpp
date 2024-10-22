#include "HelloRenderer.h"
#include "GeometryProvider.h"
#include "Renderer.h"
#include "Shader.h"
#include "Utils.h"
#include "Vertex.h"
#include "VkInit.h"
#include <cstdint>
#include <vulkan/vulkan.h>

#include "Common.h"
#include "imgui.h"

#include <iostream>
#include <vulkan/vulkan_core.h>

HelloRenderer::HelloRenderer(VulkanContext &ctx, FrameInfo &info,
                             RenderContext::Queues &queues)
    : IRenderer(ctx, info, queues)
{
    // Create Graphics Pipelines
    auto shaderStages = ShaderBuilder()
                            .SetVertexPath("assets/spirv/HelloTriangleVert.spv")
                            .SetFragmentPath("assets/spirv/HelloTriangleFrag.spv")
                            .Build(ctx);

    mGraphicsPipeline = PipelineBuilder()
                            .SetShaderStages(shaderStages)
                            .SetVertexInput<ColoredVertex>(0, VK_VERTEX_INPUT_RATE_VERTEX)
                            .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                            .SetPolygonMode(VK_POLYGON_MODE_FILL)
                            .SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE)
                            .SetColorFormat(mRenderTargetFormat)
                            .SetPushConstantSize(sizeof(glm::vec4))
                            .Build(ctx);

    mMainDeletionQueue.push_back(mGraphicsPipeline.Handle);
    mMainDeletionQueue.push_back(mGraphicsPipeline.Layout);

    // Create swapchain resources:
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
    auto &cmd = mFrame.CurrentCmd();

    VkClearValue clear{{{0.0f, 0.0f, 0.0f, 0.0f}}};

    VkRenderingAttachmentInfoKHR colorAttachment = vkinit::CreateAttachmentInfo(
        mRenderTargetView, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR, clear);

    VkRenderingInfoKHR renderingInfo =
        vkinit::CreateRenderingInfo(GetTargetSize(), colorAttachment);

    vkCmdBeginRendering(cmd, &renderingInfo);
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mGraphicsPipeline.Handle);

        common::ViewportScissor(cmd, GetTargetSize());

        for (auto &drawable : mDrawables)
        {
            std::array<VkBuffer, 1> vertexBuffers{drawable.VertexBuffer.Handle};
            std::array<VkDeviceSize, 1> offsets{0};

            vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers.data(), offsets.data());

            vkCmdBindIndexBuffer(cmd, drawable.IndexBuffer.Handle, 0,
                                 VK_INDEX_TYPE_UINT16);

            for (auto &translation : drawable.Translations)
            {
                vkCmdPushConstants(cmd, mGraphicsPipeline.Layout,
                                   VK_SHADER_STAGE_ALL_GRAPHICS, 0, sizeof(translation),
                                   &translation);
                vkCmdDrawIndexed(cmd, drawable.IndexCount, 1, 0, 0, 0);
            }
        }
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

void HelloRenderer::LoadScene(Scene &scene)
{
    for (auto &obj : scene.Objects)
    {
        auto vertPtr = obj.Provider.UnpackVertices<ColoredVertex>();
        auto idxPtr = obj.Provider.UnpackIndices<uint16_t>();

        if (vertPtr == nullptr)
        {
            std::cerr << "Unsupported vertex type.\n";
            continue;
        }

        if (idxPtr == nullptr)
        {
            std::cerr << "Unsupported index type.\n";
            continue;
        }

        mDrawables.emplace_back();
        auto &drawable = mDrawables.back();

        auto &pool = mFrame.CurrentPool();

        // Create Vertex buffer:
        {
            utils::ScopedCommand cmd(mCtx, mQueues.Graphics, pool);

            auto vertices = vertPtr->GetVertices();
            auto vertexCount = vertices.size();

            GPUBufferInfo vertInfo{
                .Usage =
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                .Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                .Size = vertexCount * sizeof(ColoredVertex),
                .Data = vertices.data(),
            };

            drawable.VertexBuffer = Buffer::CreateGPUBuffer(mCtx, cmd.Buffer, vertInfo);
            drawable.VertexCount = vertexCount;
        }

        // Create Index buffer:
        {
            utils::ScopedCommand cmd(mCtx, mQueues.Graphics, pool);

            auto indices = idxPtr->GetIndices();
            auto indexCount = indices.size();

            GPUBufferInfo idxInfo{
                .Usage =
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                .Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                .Size = indexCount * sizeof(uint16_t),
                .Data = indices.data(),
            };

            drawable.IndexBuffer = Buffer::CreateGPUBuffer(mCtx, cmd.Buffer, idxInfo);
            drawable.IndexCount = indexCount;
        }

        // Unpack instance data:
        for (auto instance : obj.Instances)
        {
            glm::vec4 trans{instance.Translation, 1.0f};

            drawable.Translations.push_back(trans);
        }
    }

    // To-do: create a dedicated deletion queue for
    // scene-depent objects:
    for (auto &drawable : mDrawables)
    {
        mMainDeletionQueue.push_back(&drawable.VertexBuffer);
        mMainDeletionQueue.push_back(&drawable.IndexBuffer);
    }
}
