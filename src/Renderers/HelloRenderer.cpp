#include "HelloRenderer.h"
#include "Descriptor.h"
#include "Pch.h"

#include "Barrier.h"
#include "Common.h"
#include "MakeImage.h"
#include "Renderer.h"
#include "Scene.h"
#include "VkEnums.h"

#include "volk.h"

#include <cstdint>
#include <ranges>
#include <string>

HelloRenderer::HelloRenderer(VulkanContext &ctx, FrameInfo &info, Camera &camera)
    : IRenderer(ctx, info, camera), mDynamicUBO(ctx, info, sizeof(mUBOData)),
      mDynamicDS(ctx, info), mSceneDeletionQueue(ctx)
{
    {
        auto stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        auto layout = DescriptorSetLayoutBuilder("HelloDynamicDescriptorSetLayout")
                          .AddUniformBuffer(0, stageFlags)
                          .Build(mCtx, mMainDeletionQueue);

        auto counts = Descriptor::PoolCounts{
            .UniformBuffer = 1,
        };

        mDynamicDS.Initialize(layout, counts);

        mDynamicDS.BeginUpdate();

        auto [buffers, sizes] = mDynamicUBO.GetBufferHandlesAndSizes();

        mDynamicDS.WriteUniformBuffer(0, buffers, sizes);
        mDynamicDS.EndUpdate();
    }

    RebuildPipelines();
    RecreateSwapchainResources();
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
            .AddDescriptorSetLayout(mDynamicDS.DescriptorSetLayout())
            .Build(mCtx, mPipelineDeletionQueue);
}

void HelloRenderer::OnUpdate([[maybe_unused]] float deltaTime)
{
    mUBOData.CameraViewProjection = mCamera.GetViewProj();
}

void HelloRenderer::OnImGui()
{
}

void HelloRenderer::OnRender([[maybe_unused]] std::optional<SceneKey> highlightedObj)
{
    auto &cmd = mFrame.CurrentCmd();

    barrier::TransferSrcToColor(cmd, mRenderTarget.Img.Handle);

    // This is not OnUpdate since, uniform buffers are per-image index
    // and as such need to be acquired after new image index is set.
    mDynamicUBO.UpdateData(&mUBOData, sizeof(mUBOData));

    auto renderInfo = common::RenderingInfo{
        .Extent = GetTargetSize(),
        .Color  = mRenderTarget.View,
    };

    common::BeginRendering(cmd, renderInfo);
    {
        mGraphicsPipeline.Bind(cmd);
        common::ViewportScissor(cmd, GetTargetSize());

        mGraphicsPipeline.BindDescriptorSet(cmd, mDynamicDS.DescriptorSet(), 0);

        for (auto &[_, drawable] : mDrawables)
        {
            VkBuffer     vertBuffer = drawable.VertexBuffer.Handle;
            VkDeviceSize vertOffset = 0;
            vkCmdBindVertexBuffers(cmd, 0, 1, &vertBuffer, &vertOffset);

            vkCmdBindIndexBuffer(cmd, drawable.IndexBuffer.Handle, 0,
                                 mGeometryLayout.IndexType);

            auto &instances = mInstanceData[drawable.Instances];

            for (auto &instance : instances)
            {
                mGraphicsPipeline.PushConstants(cmd, instance.Transform);

                vkCmdDrawIndexed(cmd, drawable.IndexCount, 1, 0, 0, 0);
            }
        }
    }
    vkCmdEndRendering(cmd);
}

void HelloRenderer::TargetToTransfer(VkCommandBuffer cmd)
{
    barrier::ColorToTransferSrc(cmd, mRenderTarget.Img.Handle);
}

void HelloRenderer::RecreateSwapchainResources()
{
    using namespace vkenum::image_usage;

    mSwapchainDeletionQueue.flush();

    // Create the render target:
    auto ScaleResolution = [this](uint32_t res) {
        return static_cast<uint32_t>(mInternalResolutionScale * static_cast<float>(res));
    };

    const VkExtent2D drawExtent{
        .width  = ScaleResolution(mCtx.Swapchain->Extent.width),
        .height = ScaleResolution(mCtx.Swapchain->Extent.height),
    };

    Image2DInfo renderTargetInfo{
        .Name   = "RenderTarget",
        .Extent = drawExtent,
        .Format = mRenderTargetFormat,
        .Usage  = TransferSrc | ColorAttachment,
        .Layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    };

    mRenderTarget =
        MakeTexture::Texture2D(mCtx, renderTargetInfo, mSwapchainDeletionQueue);
}

void HelloRenderer::LoadScene(const Scene &scene)
{
    if (scene.UpdateMeshesRequested())
        LoadMeshes(scene);

    if (scene.UpdateObjectsRequested())
        LoadObjects(scene);
}

void HelloRenderer::RenderObjectId(VkCommandBuffer cmd, float x, float y)
{
    (void)cmd;
    (void)x;
    (void)y;
}

void HelloRenderer::LoadMeshes(const Scene &scene)
{
    using namespace std::views;

    auto MakeDrawable = [&](const GeometryData &geo, const std::string &debugName,
                            SceneKey meshKey) {
        return Drawable{
            .VertexBuffer = Buffer::MakeVertex(mCtx, debugName, geo.VertexData),
            .VertexCount  = static_cast<uint32_t>(geo.VertexCount),
            .IndexBuffer  = Buffer::MakeIndex(mCtx, debugName, geo.IndexData),
            .IndexCount   = static_cast<uint32_t>(geo.IndexCount),
            .Instances    = meshKey,
        };
    };

    for (const auto &[meshKey, mesh] : scene.Meshes)
    {
        for (const auto [primIdx, prim] : enumerate(mesh.Primitives))
        {
            auto drawableKey = DrawableKey{meshKey, primIdx};

            // Already imported:
            if (mDrawables.contains(drawableKey))
                continue;

            if (mGeometryLayout == prim.Data.Layout)
            {
                const auto primName = mesh.Name + std::to_string(primIdx);

                mDrawables.try_emplace(drawableKey,
                                       MakeDrawable(prim.Data, primName, meshKey));
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
