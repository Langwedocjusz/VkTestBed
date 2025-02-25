#include "HelloRenderer.h"
#include "MeshBuffers.h"
#include "Renderer.h"
#include "Shader.h"
#include "VkInit.h"

#include "Common.h"

#include <cstdint>
#include <iostream>
#include <vulkan/vulkan.h>

HelloRenderer::HelloRenderer(VulkanContext &ctx, FrameInfo &info,
                             RenderContext::Queues &queues,
                             std::unique_ptr<Camera> &camera)
    : IRenderer(ctx, info, queues, camera), mSceneDeletionQueue(ctx)
{
    RebuildPipelines();
    CreateSwapchainResources();
}

HelloRenderer::~HelloRenderer()
{
    mSceneDeletionQueue.flush();
    mSwapchainDeletionQueue.flush();
    mPipelineDeletionQueue.flush();
    mMainDeletionQueue.flush();
}

void HelloRenderer::RebuildPipelines()
{
    mPipelineDeletionQueue.flush();

    auto shaderStages = ShaderBuilder()
                            .SetVertexPath("assets/spirv/HelloTriangleVert.spv")
                            .SetFragmentPath("assets/spirv/HelloTriangleFrag.spv")
                            .Build(mCtx);

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
            .Build(mCtx);

    mPipelineDeletionQueue.push_back(mGraphicsPipeline.Handle);
    mPipelineDeletionQueue.push_back(mGraphicsPipeline.Layout);
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

        for (auto &[_, mesh] : mMeshes)
        {
            for (auto &transform : mesh.Transforms)
            {
                for (auto &drawable : mesh.Drawables)
                {
                    std::array<VkBuffer, 1> vertexBuffers{drawable.VertexBuffer.Handle};
                    std::array<VkDeviceSize, 1> offsets{0};

                    vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers.data(),
                                           offsets.data());
                    vkCmdBindIndexBuffer(cmd, drawable.IndexBuffer.Handle, 0,
                                         mGeometryLayout.IndexType);

                    vkCmdPushConstants(cmd, mGraphicsPipeline.Layout,
                                       VK_SHADER_STAGE_ALL_GRAPHICS, 0, sizeof(transform),
                                       &transform);

                    vkCmdDrawIndexed(cmd, drawable.IndexCount, 1, 0, 0, 0);
                }
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
        .MipLevels = 1,
    };

    mRenderTarget = Image::CreateImage2D(mCtx, renderTargetInfo);
    mSwapchainDeletionQueue.push_back(mRenderTarget);

    // Create the render target view:
    mRenderTargetView = Image::CreateView2D(mCtx, mRenderTarget, mRenderTargetFormat,
                                            VK_IMAGE_ASPECT_COLOR_BIT);
    mSwapchainDeletionQueue.push_back(mRenderTargetView);
}

void HelloRenderer::LoadScene(const Scene &scene)
{
    if (scene.UpdateMeshes())
    {
        // This will only take efect on non-first runs:
        mSceneDeletionQueue.flush();
        mMeshes.clear();

        LoadMeshes(scene);
    }

    if (scene.UpdateObjects())
        LoadObjects(scene);
}

void HelloRenderer::LoadMeshes(const Scene &scene)
{
    auto &pool = mFrame.CurrentPool();

    auto CreateBuffers = [&](Drawable &drawable, const GeometryData &geo) {
        // Create Vertex buffer:
        drawable.VertexBuffer =
            VertexBuffer::Create(mCtx, mQueues.Graphics, pool, geo.VertexData);
        drawable.VertexCount = static_cast<uint32_t>(geo.VertexData.Count);

        // Create Index buffer:
        drawable.IndexBuffer =
            IndexBuffer::Create(mCtx, mQueues.Graphics, pool, geo.IndexData);
        drawable.IndexCount = static_cast<uint32_t>(geo.IndexData.Count);
    };

    for (auto &[key, sceneMesh] : scene.Meshes)
    {
        if (mMeshes.count(key) != 0)
            continue;

        if (!mGeometryLayout.IsCompatible(sceneMesh.Layout))
        {
            std::cerr << "Unsupported geometry layout.\n";
            continue;
        }

        auto &mesh = mMeshes[key];

        for (auto &geo : sceneMesh.Geometry)
        {
            auto &drawable = mesh.Drawables.emplace_back();

            CreateBuffers(drawable, geo);
        }
    }

    for (auto &[_, mesh] : mMeshes)
    {
        for (auto &drawable : mesh.Drawables)
        {
            mSceneDeletionQueue.push_back(drawable.VertexBuffer);
            mSceneDeletionQueue.push_back(drawable.IndexBuffer);
        }
    }
}

void HelloRenderer::LoadObjects(const Scene &scene)
{
    for (auto &[_, mesh] : mMeshes)
        mesh.Transforms.clear();

    for (auto &[_, obj] : scene.Objects)
    {
        // Has mesh component?
        if (auto meshKey = obj.Mesh)
        {
            // Do we have this mesh?
            if (mMeshes.count(*meshKey) == 0)
                continue;

            auto &mesh = mMeshes[*meshKey];
            mesh.Transforms.push_back(obj.Transform);
        }
    }
}
