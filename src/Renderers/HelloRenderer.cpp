#include "HelloRenderer.h"
#include "Pch.h"

#include "BufferUtils.h"
#include "Common.h"
#include "ImageUtils.h"
#include "Renderer.h"

#include "volk.h"

#include <cstdint>
#include <ranges>
#include <string>

HelloRenderer::HelloRenderer(VulkanContext &ctx, FrameInfo &info, Camera &camera)
    : IRenderer(ctx, info, camera), mDynamicUBO(ctx, info), mSceneDeletionQueue(ctx)
{
    mDynamicUBO.OnInit("HelloDynamicUBO", VK_SHADER_STAGE_VERTEX_BIT, sizeof(mUBOData));

    RebuildPipelines();
    RecreateSwapchainResources();
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

void HelloRenderer::OnRender([[maybe_unused]] std::optional<SceneKey> highlightedObj)
{
    auto &cmd = mFrame.CurrentCmd();

    // This is not OnUpdate since, uniform buffers are per-image index
    // and as such need to be acquired after new image index is set.
    mDynamicUBO.UpdateData(&mUBOData, sizeof(mUBOData));

    common::BeginRenderingColor(cmd, GetTargetSize(), mRenderTarget.View, true);
    {
        mGraphicsPipeline.Bind(cmd);
        common::ViewportScissor(cmd, GetTargetSize());

        mGraphicsPipeline.BindDescriptorSet(cmd, mDynamicUBO.DescriptorSet(), 0);

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

void HelloRenderer::RecreateSwapchainResources()
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

    mRenderTarget = MakeTexture::Texture2D(mCtx, "RenderTarget", renderTargetInfo,
                                           mSwapchainDeletionQueue);
}

void HelloRenderer::LoadScene(const Scene &scene)
{
    if (scene.UpdateMeshes())
        LoadMeshes(scene);

    if (scene.UpdateObjects())
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
