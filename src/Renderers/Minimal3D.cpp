#include "Minimal3D.h"
#include "Pch.h"

#include "Barrier.h"
#include "BufferUtils.h"
#include "Common.h"
#include "Descriptor.h"
#include "ImageLoaders.h"
#include "ImageUtils.h"
#include "Renderer.h"
#include "Sampler.h"
#include "Shader.h"
#include "VkInit.h"

#include <ranges>

Minimal3DRenderer::Minimal3DRenderer(VulkanContext &ctx, FrameInfo &info,
                                     std::unique_ptr<Camera> &camera)
    : IRenderer(ctx, info, camera), mTextureDescriptorAllocator(ctx),
      mSceneDeletionQueue(ctx)
{
    // Create descriptor set layout for sampling textures
    mTextureDescriptorSetLayout =
        DescriptorSetLayoutBuilder("Minimal3DTextureDescriptorLayout")
            .AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        VK_SHADER_STAGE_FRAGMENT_BIT)
            .Build(ctx, mMainDeletionQueue);

    // Initialize descriptor allocator for materials
    constexpr uint32_t imgPerMat = 1;

    std::vector<VkDescriptorPoolSize> poolCounts{
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imgPerMat},
    };

    mTextureDescriptorAllocator.OnInit(poolCounts);

    // Create the default texture:
    auto imgData = ImageData::SinglePixel(Pixel{255, 255, 255, 255}, false);

    mDefaultImage = TextureLoaders::LoadTexture2D(mCtx, "DefaultTexture", imgData,
                                                  VK_FORMAT_R8G8B8A8_SRGB);
    mMainDeletionQueue.push_back(mDefaultImage);

    // Create the texture sampler:
    mSampler = SamplerBuilder("Minimal3DSampler")
                   .SetMagFilter(VK_FILTER_LINEAR)
                   .SetMinFilter(VK_FILTER_LINEAR)
                   .SetAddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT)
                   .Build(mCtx, mMainDeletionQueue);

    // Build the graphics pipelines:
    RebuildPipelines();

    // Create swapchain resources:
    CreateSwapchainResources();
}

Minimal3DRenderer::~Minimal3DRenderer()
{
    mTextureDescriptorAllocator.DestroyPools();
    mSceneDeletionQueue.flush();
    mSwapchainDeletionQueue.flush();
    mPipelineDeletionQueue.flush();
    mMainDeletionQueue.flush();
}

void Minimal3DRenderer::RebuildPipelines()
{
    mPipelineDeletionQueue.flush();

    auto coloredShaderStages =
        ShaderBuilder()
            .SetVertexPath("assets/spirv/Minimal3DColoredVert.spv")
            .SetFragmentPath("assets/spirv/Minimal3DColoredFrag.spv")
            .Build(mCtx);

    mColoredPipeline =
        PipelineBuilder("Minimal3DColoredPipeline")
            .SetShaderStages(coloredShaderStages)
            .SetVertexInput(mColoredLayout.VertexLayout, 0, VK_VERTEX_INPUT_RATE_VERTEX)
            .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetPolygonMode(VK_POLYGON_MODE_FILL)
            .SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .SetColorFormat(mRenderTargetFormat)
            .SetPushConstantSize(sizeof(glm::mat4))
            .AddDescriptorSetLayout(mCamera->DescriptorSetLayout())
            .EnableDepthTest()
            .SetDepthFormat(mDepthFormat)
            .Build(mCtx, mPipelineDeletionQueue);

    auto textuedShaderStages =
        ShaderBuilder()
            .SetVertexPath("assets/spirv/Minimal3DTexturedVert.spv")
            .SetFragmentPath("assets/spirv/Minimal3DTexturedFrag.spv")
            .Build(mCtx);

    mTexturedPipeline =
        PipelineBuilder("Minimal3DTexturedPipeline")
            .SetShaderStages(textuedShaderStages)
            .SetVertexInput(mTexturedLayout.VertexLayout, 0, VK_VERTEX_INPUT_RATE_VERTEX)
            .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetPolygonMode(VK_POLYGON_MODE_FILL)
            .SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .SetColorFormat(mRenderTargetFormat)
            .SetPushConstantSize(sizeof(PushConstantData))
            .AddDescriptorSetLayout(mCamera->DescriptorSetLayout())
            .AddDescriptorSetLayout(mTextureDescriptorSetLayout)
            .EnableDepthTest()
            .SetDepthFormat(mDepthFormat)
            .Build(mCtx, mPipelineDeletionQueue);
}

void Minimal3DRenderer::OnUpdate([[maybe_unused]] float deltaTime)
{
}

void Minimal3DRenderer::OnImGui()
{
}

void Minimal3DRenderer::OnRender()
{
    auto &cmd = mFrame.CurrentCmd();

    barrier::ImageBarrierDepthToRender(cmd, mDepthBuffer.Handle);

    VkClearValue clear;
    clear.color = {{0.0f, 0.0f, 0.0f, 0.0f}};

    auto colorAttachment = vkinit::CreateAttachmentInfo(
        mRenderTargetView, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, clear);

    VkClearValue depthClear;
    depthClear.depthStencil = {1.0f, 0};

    auto depthAttachment = vkinit::CreateAttachmentInfo(
        mDepthBufferView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, depthClear);

    VkRenderingInfoKHR renderingInfo =
        vkinit::CreateRenderingInfo(GetTargetSize(), colorAttachment, depthAttachment);

    vkCmdBeginRendering(cmd, &renderingInfo);
    {
        // 1. Colored vertices pass:
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mColoredPipeline.Handle);

        common::ViewportScissor(cmd, GetTargetSize());

        // To-do: figure out a better way of doing this:
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                mColoredPipeline.Layout, 0, 1, mCamera->DescriptorSet(),
                                0, nullptr);

        for (auto &[_, drawable] : mColoredDrawables)
        {
            VkBuffer vertBuffer = drawable.VertexBuffer.Handle;
            VkDeviceSize vertOffset = 0;
            vkCmdBindVertexBuffers(cmd, 0, 1, &vertBuffer, &vertOffset);

            vkCmdBindIndexBuffer(cmd, drawable.IndexBuffer.Handle, 0,
                                 mColoredLayout.IndexType);

            for (auto &transform : drawable.Instances)
            {

                vkCmdPushConstants(cmd, mColoredPipeline.Layout,
                                   VK_SHADER_STAGE_ALL_GRAPHICS, 0, sizeof(transform),
                                   &transform);
                vkCmdDrawIndexed(cmd, drawable.IndexCount, 1, 0, 0, 0);
            }
        }

        // 2. Textured vertices pass:
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mTexturedPipeline.Handle);

        common::ViewportScissor(cmd, GetTargetSize());

        // To-do: figure out a better way of doing this:
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                mTexturedPipeline.Layout, 0, 1, mCamera->DescriptorSet(),
                                0, nullptr);

        for (auto &[_, drawable] : mTexturedDrawables)
        {
            VkBuffer vertBuffer = drawable.VertexBuffer.Handle;
            VkDeviceSize vertOffset = 0;
            vkCmdBindVertexBuffers(cmd, 0, 1, &vertBuffer, &vertOffset);

            vkCmdBindIndexBuffer(cmd, drawable.IndexBuffer.Handle, 0,
                                 mTexturedLayout.IndexType);

            auto &material = mMaterials.at(drawable.Material);

            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    mTexturedPipeline.Layout, 1, 1,
                                    &material.DescriptorSet, 0, nullptr);

            for (auto &transform : drawable.Instances)
            {

                PushConstantData pcData{
                    .AlphaCutoff = glm::vec4(material.AlphaCutoff),
                    .Transform = transform,
                };

                vkCmdPushConstants(cmd, mTexturedPipeline.Layout,
                                   VK_SHADER_STAGE_ALL_GRAPHICS, 0, sizeof(pcData),
                                   &pcData);
                vkCmdDrawIndexed(cmd, drawable.IndexCount, 1, 0, 0, 0);
            }
        }
    }
    vkCmdEndRendering(cmd);
}

void Minimal3DRenderer::CreateSwapchainResources()
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
    };

    mRenderTarget = MakeImage::Image2D(mCtx, "RenderTarget", renderTargetInfo);
    mSwapchainDeletionQueue.push_back(mRenderTarget);

    // Create the render target view:
    mRenderTargetView = MakeView::View2D(mCtx, "RenderTargetView", mRenderTarget,
                                         mRenderTargetFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    mSwapchainDeletionQueue.push_back(mRenderTargetView);

    // Create depth buffer:
    Image2DInfo depthBufferInfo{
        .Extent = drawExtent,
        .Format = mDepthFormat,
        .Tiling = VK_IMAGE_TILING_OPTIMAL,
        .Usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .MipLevels = 1,
    };

    mDepthBuffer = MakeImage::Image2D(mCtx, "DepthBuffer", depthBufferInfo);
    mDepthBufferView = MakeView::View2D(mCtx, "DepthBufferView", mDepthBuffer,
                                        mDepthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

    mSwapchainDeletionQueue.push_back(mDepthBuffer);
    mSwapchainDeletionQueue.push_back(mDepthBufferView);
}

void Minimal3DRenderer::LoadScene(const Scene &scene)
{
    if (scene.UpdateMeshes())
        LoadMeshes(scene);

    if (scene.UpdateImages())
        LoadImages(scene);

    if (scene.UpdateMaterials())
        LoadMaterials(scene);

    if (scene.UpdateMeshMaterials())
        LoadMeshMaterials(scene);

    if (scene.UpdateObjects())
        LoadObjects(scene);
}

void Minimal3DRenderer::LoadMeshes(const Scene &scene)
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

        mSceneDeletionQueue.push_back(drawable.VertexBuffer);
        mSceneDeletionQueue.push_back(drawable.IndexBuffer);
    };

    for (const auto &[meshKey, mesh] : scene.Meshes)
    {
        for (const auto [primIdx, prim] : enumerate(mesh.Primitives))
        {
            auto drawableKey = DrawableKey{meshKey, primIdx};

            // Already imported:
            if (mTexturedDrawables.count(drawableKey) != 0 ||
                mTexturedDrawables.count(drawableKey) != 0)
                continue;

            const auto primName = mesh.Name + std::to_string(primIdx);

            if (mColoredLayout.IsCompatible(prim.Data.Layout))
            {
                auto &drawable = mColoredDrawables[drawableKey];

                CreateBuffers(drawable, prim.Data, primName);
            }

            if (mTexturedLayout.IsCompatible(prim.Data.Layout))
            {
                auto &drawable = mTexturedDrawables[drawableKey];

                CreateBuffers(drawable, prim.Data, primName);
            }
        }
    }
}

void Minimal3DRenderer::LoadImages(const Scene &scene)
{
    for (auto &[key, imgData] : scene.Images)
    {
        if (mImages.count(key) != 0)
            continue;

        auto &texture = mImages[key];

        texture = TextureLoaders::LoadTexture2DMipped(mCtx, "MaterialTexture", imgData,
                                                      VK_FORMAT_R8G8B8A8_SRGB);
        mSceneDeletionQueue.push_back(texture);
    }
}

void Minimal3DRenderer::LoadMaterials(const Scene &scene)
{
    for (auto &[key, sceneMat] : scene.Materials)
    {
        auto &mat = mMaterials[key];

        // Allocate descripor set only on first load:
        if (mMaterials.count(key) == 0)
        {
            mat.DescriptorSet =
                mTextureDescriptorAllocator.Allocate(mTextureDescriptorSetLayout);
        }

        // Update the alpha cutoff:
        mat.AlphaCutoff = sceneMat.AlphaCutoff;

        // Retrieve albedo texture if possible
        auto &texture = mDefaultImage;

        if (auto albedo = sceneMat.Albedo)
        {
            if (mImages.count(*albedo) != 0)
                texture = mImages[*albedo];
        }

        // Update the descriptor set:
        DescriptorUpdater(mat.DescriptorSet)
            .WriteImageSampler(0, texture.View, mSampler)
            .Update(mCtx);
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
                auto &drawable = mTexturedDrawables[drawableKey];

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
                mColoredDrawables[drawableKey].Instances.push_back(obj.Transform);
            }

            else if (mTexturedDrawables.count(drawableKey) != 0)
            {
                mTexturedDrawables[drawableKey].Instances.push_back(obj.Transform);
            }
        }
    }
}
