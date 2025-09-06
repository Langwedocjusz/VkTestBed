#include "HelloRenderer.h"
#include "Pch.h"

#include "BufferUtils.h"
#include "ImageUtils.h"
#include "Renderer.h"
#include "VkInit.h"

#include "Common.h"

#include <cstdint>
#include <ranges>
#include <string>
#include <vulkan/vulkan.h>

HelloRenderer::HelloRenderer(VulkanContext &ctx, FrameInfo &info, Camera &camera)
    : IRenderer(ctx, info, camera), mDynamicUBO(ctx, info), mSceneDeletionQueue(ctx)
{
    mDynamicUBO.OnInit("HelloDynamicUBO", VK_SHADER_STAGE_VERTEX_BIT, sizeof(mUBOData));

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

    mGraphicsPipeline =
        PipelineBuilder("HelloRendererPipeline")
            .SetShaderPathVertex("assets/spirv/HelloTriangleVert.spv")
            .SetShaderPathFragment("assets/spirv/HelloTriangleFrag.spv")
            .SetVertexInput(mGeometryLayout.VertexLayout, 0, VK_VERTEX_INPUT_RATE_VERTEX)
            .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetPolygonMode(VK_POLYGON_MODE_FILL)
            .SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .SetColorFormat(mRenderTargetFormat)
            .SetPushConstantSize(sizeof(glm::mat4))
            .AddDescriptorSetLayout(mDynamicUBO.DescriptorSetLayout())
            .Build(mCtx, mPipelineDeletionQueue);
}

void HelloRenderer::OnUpdate([[maybe_unused]] float deltaTime)
{
    mUBOData.CameraViewProjection = mCamera.GetViewProj();
}

void HelloRenderer::OnImGui()
{
}

void HelloRenderer::OnRender()
{
    auto &cmd = mFrame.CurrentCmd();

    // This is not OnUpdate since, uniform buffers are per-image index
    // and as such need to be acquired after new image index is set.
    mDynamicUBO.UpdateData(&mUBOData, sizeof(mUBOData));

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
                                mGraphicsPipeline.Layout, 0, 1,
                                mDynamicUBO.DescriptorSet(), 0, nullptr);

        for (auto &[_, drawable] : mDrawables)
        {
            VkBuffer vertBuffer = drawable.VertexBuffer.Handle;
            VkDeviceSize vertOffset = 0;
            vkCmdBindVertexBuffers(cmd, 0, 1, &vertBuffer, &vertOffset);

            vkCmdBindIndexBuffer(cmd, drawable.IndexBuffer.Handle, 0,
                                 mGeometryLayout.IndexType);

            auto &instances = mInstanceData[drawable.Instances];

            for (auto &instance : instances)
            {

                vkCmdPushConstants(cmd, mGraphicsPipeline.Layout,
                                   VK_SHADER_STAGE_ALL_GRAPHICS, 0,
                                   sizeof(instance.Transform), &instance.Transform);

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

    VkExtent2D drawExtent{
        .width = width,
        .height = height,
    };

    VkImageUsageFlags drawUsage{};
    drawUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    Image2DInfo renderTargetInfo{
        .Extent = drawExtent,
        .Format = mRenderTargetFormat,
        .Tiling = VK_IMAGE_TILING_OPTIMAL,
        .Usage = drawUsage,
        .MipLevels = 1,
        .Layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    };

    mRenderTarget = MakeImage::Image2D(mCtx, "RenderTarget", renderTargetInfo);
    mSwapchainDeletionQueue.push_back(mRenderTarget);

    // Create the render target view:
    mRenderTargetView = MakeView::View2D(mCtx, "RenderTargetView", mRenderTarget,
                                         mRenderTargetFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    mSwapchainDeletionQueue.push_back(mRenderTargetView);
}

void HelloRenderer::LoadScene(const Scene &scene)
{
    if (scene.UpdateMeshes())
        LoadMeshes(scene);

    if (scene.UpdateObjects())
        LoadObjects(scene);
}

void HelloRenderer::LoadMeshes(const Scene &scene)
{
    using namespace std::views;

    auto CreateBuffers = [&](Drawable &drawable, const GeometryData &geo,
                             const std::string &debugName) {
        // Create Vertex buffer:
        drawable.VertexBuffer = MakeBuffer::Vertex(mCtx, debugName, geo.VertexData);
        drawable.VertexCount = static_cast<uint32_t>(geo.VertexData.Count);

        // Create Index buffer:
        drawable.IndexBuffer = MakeBuffer::Index(mCtx, debugName, geo.IndexData);
        drawable.IndexCount = static_cast<uint32_t>(geo.IndexData.Count);

        // Update deletion queue:
        mSceneDeletionQueue.push_back(drawable.VertexBuffer);
        mSceneDeletionQueue.push_back(drawable.IndexBuffer);
    };

    for (const auto &[meshKey, mesh] : scene.Meshes)
    {
        for (const auto [primIdx, prim] : enumerate(mesh.Primitives))
        {
            auto drawableKey = DrawableKey{meshKey, primIdx};

            // Already imported:
            if (mDrawables.count(drawableKey) != 0)
                continue;

            if (mGeometryLayout.IsCompatible(prim.Data.Layout))
            {
                auto &drawable = mDrawables[drawableKey];

                const auto primName = mesh.Name + std::to_string(primIdx);

                CreateBuffers(drawable, prim.Data, primName);
                drawable.Instances = meshKey;
            }
        }
    }
}

void HelloRenderer::LoadObjects(const Scene &scene)
{
    for (auto &[_, instances] : mInstanceData)
        instances.clear();

    for (const auto &[key, obj] : scene.Objects)
    {
        if (!obj.Mesh.has_value())
            continue;

        auto meshKey = *obj.Mesh;

        auto &instances = mInstanceData[meshKey];
        instances.push_back(InstanceData{.Transform = obj.Transform});
    }
}
