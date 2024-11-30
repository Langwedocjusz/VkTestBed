#include "HelloRenderer.h"
#include "GeometryProvider.h"
#include "MeshBuffers.h"
#include "Renderer.h"
#include "Shader.h"
#include "VkInit.h"

#include "Common.h"

#include <cstdint>
#include <iostream>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

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

    mGraphicsPipeline =
        PipelineBuilder()
            .SetShaderStages(shaderStages)
            .SetVertexInput(mGeometryLayout.VertexLayout, 0, VK_VERTEX_INPUT_RATE_VERTEX)
            .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetPolygonMode(VK_POLYGON_MODE_FILL)
            .SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
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
                                 mGeometryLayout.IndexType);

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
    auto &pool = mFrame.CurrentPool();

    for (auto &provider : scene.GeoProviders)
    {
        if (!mGeometryLayout.IsCompatible(provider.Layout))
        {
            std::cerr << "Unsupported geometry layout.\n";
            continue;
        }

        auto &drawable = mDrawables.emplace_back();
        auto geo = provider.GetGeometry();

        // Create Vertex buffer:
        drawable.VertexBuffer =
            VertexBuffer::Create(mCtx, mQueues.Graphics, pool, geo.VertexData);
        drawable.VertexCount = geo.VertexData.Count;

        // Create Index buffer:
        drawable.IndexBuffer =
            IndexBuffer::Create(mCtx, mQueues.Graphics, pool, geo.IndexData);
        drawable.IndexCount = geo.IndexData.Count;
    }

    for (auto &drawable : mDrawables)
    {
        mSceneDeletionQueue.push_back(&drawable.VertexBuffer);
        mSceneDeletionQueue.push_back(&drawable.IndexBuffer);
    }
}

void HelloRenderer::LoadInstances(Scene &scene)
{
    for (auto &drawable : mDrawables)
        drawable.Transforms.clear();

    for (auto &obj : scene.Objects)
    {
        if (!obj.has_value())
            continue;

        bool hasGeo = obj->GeometryId.has_value();

        if (!hasGeo)
            continue;

        size_t geoId = obj->GeometryId.value();

        auto &geo = scene.GeoProviders[geoId];

        if (!mGeometryLayout.IsCompatible(geo.Layout))
        {
            std::cerr << "Unsupported geometry layout.\n";
            continue;
        }

        auto &drawable = mDrawables[geoId];
        drawable.Transforms.push_back(obj->Transform);
    }
}
