#include "Minimal3D.h"
#include "Pch.h"

#include "Barrier.h"
#include "Common.h"
#include "Descriptor.h"
#include "Renderer.h"
#include "Sampler.h"
#include "VkEnums.h"

#include <ranges>

Minimal3DRenderer::Minimal3DRenderer(VulkanContext &ctx, FrameInfo &info, Camera &camera)
    : IRenderer(ctx, info, camera)
    , mTextureDescriptorAllocator(ctx)
    , mDynamicUBO(ctx, info, sizeof(mUBOData))
    , mDynamicDS(ctx, info)
    , mSceneDeletionQueue(ctx)
{
    // Create the texture sampler:
    mSampler = SamplerBuilder("Minimal3DSampler")
                   .SetMagFilter(VK_FILTER_LINEAR)
                   .SetMinFilter(VK_FILTER_LINEAR)
                   .SetAddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT)
                   .Build(mCtx, mMainDeletionQueue);

    // Initialize ubo dynamic descriptor set:
    {
        // clang-format off
        auto layout = 
            DescriptorSetLayoutBuilder("Minimal3DDynamicDescriptorSetLayout")
                .AddUniformBuffer(0, VK_SHADER_STAGE_VERTEX_BIT)
                .Build(mCtx, mMainDeletionQueue);
        // clang-format on

        auto counts = Descriptor::PoolCounts{
            .UniformBuffer = 1,
        };

        mDynamicDS.Initialize(layout, counts);

        mDynamicDS.BeginUpdate();

        auto [buffers, sizes] = mDynamicUBO.GetBufferHandlesAndSizes();
        mDynamicDS.WriteUniformBuffer(0, buffers, sizes);

        mDynamicDS.EndUpdate();
    }

    // Create descriptor set layout for sampling textures
    // clang-format off
    mTextureDescriptorSetLayout = 
        DescriptorSetLayoutBuilder("Minimal3DTextureDescriptorLayout")
            .AddCombinedSampler(0, VK_SHADER_STAGE_FRAGMENT_BIT)
            .Build(ctx, mMainDeletionQueue);
    // clang-format on

    // Initialize descriptor allocator for materials:
    mTextureDescriptorAllocator.OnInit({
        .CombinedImageSampler = 1,
    });

    // Create the default texture:
    auto imgData = ImageData::SinglePixel(Pixel{255, 255, 255, 255}, false);

    mDefaultImage = Texture::FromData(mCtx, "DefaultTexture", imgData);

    // Build the graphics pipelines:
    RebuildPipelines();

    // Create swapchain resources:
    RecreateSwapchainResources();
}

void Minimal3DRenderer::RebuildPipelines()
{
    mPipelineDeletionQueue.flush();

    // clang-format off
    mColoredPipeline = 
        PipelineBuilder("Minimal3DColoredPipeline")
            .SetShaderPathVertex("assets/spirv/Minimal3DColoredVert.spv")
            .SetShaderPathFragment("assets/spirv/Minimal3DColoredFrag.spv")
            .SetVertexInput(mColoredLayout.VertexLayout, 0, VK_VERTEX_INPUT_RATE_VERTEX)
            .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetPolygonMode(VK_POLYGON_MODE_FILL)
            .SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .SetColorFormat(mRenderTargetFormat)
            .OverrideDescriptor(0, mDynamicDS.DescriptorSetLayout())
            .EnableDepthTest()
            .SetDepthFormat(mDepthFormat)
            .Build(mCtx, mPipelineDeletionQueue);

    mTexturedPipeline = 
        PipelineBuilder("Minimal3DTexturedPipeline")
            .SetShaderPathVertex("assets/spirv/Minimal3DTexturedVert.spv")
            .SetShaderPathFragment("assets/spirv/Minimal3DTexturedFrag.spv")
            .SetVertexInput(mTexturedLayout.VertexLayout, 0, VK_VERTEX_INPUT_RATE_VERTEX)
            .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetPolygonMode(VK_POLYGON_MODE_FILL)
            .SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .SetColorFormat(mRenderTargetFormat)
            .OverrideDescriptor(0, mDynamicDS.DescriptorSetLayout())
            .OverrideDescriptor(1, mTextureDescriptorSetLayout)
            .EnableDepthTest()
            .SetDepthFormat(mDepthFormat)
            .Build(mCtx, mPipelineDeletionQueue);
    // clang-format on
}

void Minimal3DRenderer::OnUpdate([[maybe_unused]] float deltaTime)
{
    mUBOData.CameraViewProjection = mCamera.GetViewProj();
}

void Minimal3DRenderer::OnImGui()
{
}

void Minimal3DRenderer::OnRender([[maybe_unused]] std::optional<SceneKey> highlightedObj)
{
    auto &cmd = mFrame.CurrentCmd();

    barrier::TransferSrcToColor(cmd, mRenderTarget->Img.Handle);

    // This is not OnUpdate since, uniform buffers are per-image index
    // and as such need to be acquired after new image index is set.
    mDynamicUBO.UpdateData(&mUBOData, sizeof(mUBOData));

    auto renderInfo = common::RenderingInfo{
        .Extent          = GetTargetSize(),
        .Color           = mRenderTarget->View,
        .Depth           = mDepthBuffer->View,
        .DepthHasStencil = false,
    };

    common::BeginRendering(cmd, renderInfo);
    {
        // 1. Colored vertices pass:
        mColoredPipeline.Bind(cmd);
        common::ViewportScissor(cmd, GetTargetSize());

        mColoredPipeline.BindDescriptorSet(cmd, mDynamicDS.DescriptorSet(), 0);

        for (auto &[_, drawable] : mColoredDrawables)
        {
            VkBuffer     vertBuffer = drawable.VertexBuffer.Handle;
            VkDeviceSize vertOffset = 0;
            vkCmdBindVertexBuffers(cmd, 0, 1, &vertBuffer, &vertOffset);

            vkCmdBindIndexBuffer(cmd, drawable.IndexBuffer.Handle, 0, mColoredLayout.IndexType);

            for (auto &transform : drawable.Instances)
            {
                mColoredPipeline.PushConstants(cmd, transform);

                vkCmdDrawIndexed(cmd, drawable.IndexCount, 1, 0, 0, 0);
            }
        }

        // 2. Textured vertices pass:
        mTexturedPipeline.Bind(cmd);
        common::ViewportScissor(cmd, GetTargetSize());

        mTexturedPipeline.BindDescriptorSet(cmd, mDynamicDS.DescriptorSet(), 0);

        for (auto &[_, drawable] : mTexturedDrawables)
        {
            VkBuffer     vertBuffer = drawable.VertexBuffer.Handle;
            VkDeviceSize vertOffset = 0;
            vkCmdBindVertexBuffers(cmd, 0, 1, &vertBuffer, &vertOffset);

            vkCmdBindIndexBuffer(cmd, drawable.IndexBuffer.Handle, 0, mTexturedLayout.IndexType);

            auto &material = mMaterials.at(drawable.Material);
            mTexturedPipeline.BindDescriptorSet(cmd, material.DescriptorSet, 1);

            for (auto &transform : drawable.Instances)
            {
                struct PCData {
                    // Vec4 because of std430 alignment rules:
                    glm::vec4 AlphaCutoff;
                    glm::mat4 Transform;
                };

                // clang-format off
                mTexturedPipeline.PushConstants(cmd, PCData{
                    .AlphaCutoff = glm::vec4(material.AlphaCutoff),
                    .Transform   = transform,
                });
                // clang-format on

                vkCmdDrawIndexed(cmd, drawable.IndexCount, 1, 0, 0, 0);
            }
        }
    }
    vkCmdEndRendering(cmd);
}

void Minimal3DRenderer::TargetToTransfer(VkCommandBuffer cmd)
{
    barrier::ColorToTransferSrc(cmd, mRenderTarget->Img.Handle);
}

void Minimal3DRenderer::RecreateSwapchainResources()
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

    // clang-format off
    mRenderTarget = Texture::Make2D(mCtx, {
        .Name   = "RenderTarget",
        .Extent = drawExtent,
        .Format = mRenderTargetFormat,
        .Usage  = TransferSrc | ColorAttachment,
        .Layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    });

    mDepthBuffer = Texture::Make2D(mCtx, {
        .Name   = "DepthBuffer",
        .Extent = drawExtent,
        .Format = mDepthFormat,
        .Usage  = DepthStencil,
        .Layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
    });
    // clang-format on
}

void Minimal3DRenderer::LoadScene(const Scene &scene)
{
    if (scene.UpdateMeshesRequested())
        LoadMeshes(scene);

    if (scene.UpdateImagesRequested())
        LoadImages(scene);

    if (scene.UpdateMaterialsRequested())
        LoadMaterials(scene);

    if (scene.UpdateMeshMaterialsRequested())
        LoadMeshMaterials(scene);

    if (scene.UpdateObjectsRequested())
        LoadObjects(scene);
}

void Minimal3DRenderer::RenderObjectId(VkCommandBuffer cmd, float x, float y)
{
    (void)cmd;
    (void)x;
    (void)y;
}

void Minimal3DRenderer::LoadMeshes(const Scene &scene)
{
    using namespace std::views;

    auto MakeDrawable = [&](const GeometryData &geo, const std::string &debugName) {
        return Drawable{
            .VertexBuffer = Buffer::MakeVertex(mCtx, debugName, geo.VertexData),
            .VertexCount  = static_cast<uint32_t>(geo.VertexCount),
            .IndexBuffer  = Buffer::MakeIndex(mCtx, debugName, geo.IndexData),
            .IndexCount   = static_cast<uint32_t>(geo.IndexCount),
            .Material     = 0,
            .Instances    = {},
        };
    };

    for (const auto &[meshKey, mesh] : scene.Meshes)
    {
        for (const auto [primIdx, prim] : enumerate(mesh.Primitives))
        {
            auto drawableKey = DrawableKey{meshKey, primIdx};

            // Already imported:
            if (mColoredDrawables.contains(drawableKey) || mTexturedDrawables.contains(drawableKey))
                continue;

            // Create new drawable:
            const auto primName = mesh.Name + std::to_string(primIdx);

            if (mColoredLayout == prim.Data.Layout)
            {
                mColoredDrawables.try_emplace(drawableKey, MakeDrawable(prim.Data, primName));
            }

            if (mTexturedLayout == prim.Data.Layout)
            {
                mTexturedDrawables.try_emplace(drawableKey, MakeDrawable(prim.Data, primName));
            }
        }
    }
}

void Minimal3DRenderer::LoadImages(const Scene &scene)
{
    for (auto &[key, imgData] : scene.Images)
    {
        if (!mTextures.contains(key))
            continue;

        mTextures.try_emplace(key, Texture::FromData(mCtx, "MaterialTexture", imgData));
    }
}

void Minimal3DRenderer::LoadMaterials(const Scene &scene)
{
    for (auto &[key, sceneMat] : scene.Materials)
    {
        const bool firstLoad = mMaterials.count(key) == 0;
        auto      &mat       = mMaterials[key];

        // Allocate descripor set only on first load:
        if (firstLoad)
        {
            mat.DescriptorSet = mTextureDescriptorAllocator.Allocate(mTextureDescriptorSetLayout);
        }

        // Update the alpha cutoff:
        mat.AlphaCutoff = sceneMat.AlphaCutoff;

        // Retrieve albedo texture if possible:
        auto &texture = [&]() -> Texture & {
            auto albedo = sceneMat.Albedo;

            if (albedo && mTextures.contains(*albedo))
            {
                return mTextures.at(*albedo);
            }

            return *mDefaultImage;
        }();

        // Update the descriptor set:
        // clang-format off
        DescriptorUpdater(mat.DescriptorSet)
            .WriteCombinedSampler(0, texture.View, mSampler)
            .Update(mCtx);
        // clang-format on
    }
}

void Minimal3DRenderer::LoadMeshMaterials(const Scene &scene)
{
    using namespace std::views;

    for (const auto &[meshKey, mesh] : scene.Meshes)
    {
        for (const auto [primIdx, prim] : enumerate(mesh.Primitives))
        {
            auto drawableKey = DrawableKey{meshKey, primIdx};

            if (mTexturedDrawables.count(drawableKey) != 0)
            {
                auto &drawable = mTexturedDrawables.at(drawableKey);

                if (prim.Material)
                    drawable.Material = *prim.Material;
            }
        }
    }
}

void Minimal3DRenderer::LoadObjects(const Scene &scene)
{
    using namespace std::views;

    for (auto &[_, drawable] : mColoredDrawables)
        drawable.Instances.clear();

    for (auto &[_, drawable] : mTexturedDrawables)
        drawable.Instances.clear();

    for (const auto &[key, obj] : scene.Objects)
    {
        if (!obj.Mesh.has_value())
            continue;

        auto meshKey = *obj.Mesh;

        for (const auto [primIdx, _] : enumerate(scene.Meshes.at(meshKey).Primitives))
        {
            auto drawableKey = DrawableKey{meshKey, primIdx};

            if (mColoredDrawables.count(drawableKey) != 0)
            {
                mColoredDrawables.at(drawableKey).Instances.push_back(obj.Transform);
            }

            else if (mTexturedDrawables.count(drawableKey) != 0)
            {
                mTexturedDrawables.at(drawableKey).Instances.push_back(obj.Transform);
            }
        }
    }
}
