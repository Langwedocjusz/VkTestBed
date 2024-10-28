#include "HelloRenderer.h"
#include "GeometryProvider.h"
#include "Renderer.h"
#include "Shader.h"
#include "Utils.h"
#include "Vertex.h"
#include "VkInit.h"

#include "Common.h"

#include <cstdint>
#include <iostream>
#include <vulkan/vulkan.h>

HelloRenderer::HelloRenderer(VulkanContext &ctx, FrameInfo &info,
                             RenderContext::Queues &queues,
                             std::unique_ptr<Camera> &camera)
    : IRenderer(ctx, info, queues, camera)
{
    // Create Graphics Pipelines
    auto shaderStages = ShaderBuilder()
                            .SetVertexPath("assets/spirv/HelloTriangleVert.spv")
                            .SetFragmentPath("assets/spirv/HelloTriangleFrag.spv")
                            .Build(ctx);

    mGraphicsPipeline = PipelineBuilder()
                            .SetShaderStages(shaderStages)
                            .SetVertexInput<Vertex_PC>(0, VK_VERTEX_INPUT_RATE_VERTEX)
                            .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                            .SetPolygonMode(VK_POLYGON_MODE_FILL)
                            .SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE)
                            .SetColorFormat(mRenderTargetFormat)
                            .SetPushConstantSize(sizeof(glm::mat4))
                            .AddDescriptorSetLayout(mCamera->DescriptorSetLayout())
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
    mSceneDeletionQueue.flush();
}

void HelloRenderer::OnUpdate([[maybe_unused]] float deltaTime)
{
}

void HelloRenderer::OnImGui()
{
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

        // To-do: figure out a better way of doing this:
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                mGraphicsPipeline.Layout, 0, 1, mCamera->DescriptorSet(),
                                0, nullptr);

        for (auto &drawable : mDrawables)
        {
            std::array<VkBuffer, 1> vertexBuffers{drawable.VertexBuffer.Handle};
            std::array<VkDeviceSize, 1> offsets{0};

            vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers.data(), offsets.data());

            vkCmdBindIndexBuffer(cmd, drawable.IndexBuffer.Handle, 0,
                                 VK_INDEX_TYPE_UINT16);

            for (auto &transform : drawable.Transforms)
            {
                vkCmdPushConstants(cmd, mGraphicsPipeline.Layout,
                                   VK_SHADER_STAGE_ALL_GRAPHICS, 0, sizeof(transform),
                                   &transform);
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
    if (scene.GlobalUpdate)
    {
        // This will only take efect on non-first runs:
        mSceneDeletionQueue.flush();
        mDrawables.clear();

        LoadProviders(scene);
    }

    LoadInstances(scene);
}

void HelloRenderer::LoadProviders(Scene &scene)
{
    for (auto &obj : scene.Objects)
    {
        auto vertOpt = obj.Provider.UnpackVertices<Vertex_PC>();
        auto idxOpt = obj.Provider.UnpackIndices<uint16_t>();

        if (!vertOpt.has_value())
        {
            std::cerr << "Unsupported vertex type.\n";
            continue;
        }

        if (!idxOpt.has_value())
        {
            std::cerr << "Unsupported index type.\n";
            continue;
        }

        auto vertFn = vertOpt.value();
        auto idxFn = idxOpt.value();

        mDrawables.emplace_back();
        auto &drawable = mDrawables.back();

        auto &pool = mFrame.CurrentPool();

        // Create Vertex buffer:
        {
            utils::ScopedCommand cmd(mCtx, mQueues.Graphics, pool);

            auto vertices = vertFn();
            auto vertexCount = vertices.size();

            GPUBufferInfo vertInfo{
                .Usage =
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                .Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                .Size = vertexCount * sizeof(Vertex_PC),
                .Data = vertices.data(),
            };

            drawable.VertexBuffer = Buffer::CreateGPUBuffer(mCtx, cmd.Buffer, vertInfo);
            drawable.VertexCount = vertexCount;
        }

        // Create Index buffer:
        {
            utils::ScopedCommand cmd(mCtx, mQueues.Graphics, pool);

            auto indices = idxFn();
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
    }

    for (auto &drawable : mDrawables)
    {
        mSceneDeletionQueue.push_back(&drawable.VertexBuffer);
        mSceneDeletionQueue.push_back(&drawable.IndexBuffer);
    }
}

void HelloRenderer::LoadInstances(Scene &scene)
{
    size_t drawableId = 0;

    for (auto &obj : scene.Objects)
    {
        auto vertOpt = obj.Provider.UnpackVertices<Vertex_PC>();
        auto idxOpt = obj.Provider.UnpackIndices<uint16_t>();

        if (!vertOpt.has_value())
        {
            std::cerr << "Unsupported vertex type.\n";
            continue;
        }

        if (!idxOpt.has_value())
        {
            std::cerr << "Unsupported index type.\n";
            continue;
        }

        auto &drawable = mDrawables[drawableId];

        if (obj.UpdateInstances)
        {
            drawable.Transforms.clear();

            // Unpack instance data:
            for (auto instance : obj.Instances)
            {
                drawable.Transforms.push_back(instance.GetTransform());
            }
        }

        drawableId++;
    }
}
