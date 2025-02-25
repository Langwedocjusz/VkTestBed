#include "Minimal3D.h"
#include "Barrier.h"
#include "Common.h"
#include "Descriptor.h"
#include "ImageLoaders.h"
#include "MeshBuffers.h"
#include "Renderer.h"
#include "Sampler.h"
#include "Shader.h"
#include "VkInit.h"

#include <cstdint>
#include <ranges>
#include <vulkan/vulkan.h>

Minimal3DRenderer::Minimal3DRenderer(VulkanContext &ctx, FrameInfo &info,
                                     RenderContext::Queues &queues,
                                     std::unique_ptr<Camera> &camera)
    : IRenderer(ctx, info, queues, camera), mTextureDescriptorAllocator(ctx),
      mSceneDeletionQueue(ctx)
{
    // Create descriptor set layout for sampling textures
    mTextureDescriptorSetLayout =
        DescriptorSetLayoutBuilder()
            .AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        VK_SHADER_STAGE_FRAGMENT_BIT)
            .Build(ctx);

    mMainDeletionQueue.push_back(mTextureDescriptorSetLayout);

    // Initialize descriptor allocator for materials
    constexpr uint32_t imgPerMat = 1;

    std::vector<VkDescriptorPoolSize> poolCounts{
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imgPerMat},
    };

    mTextureDescriptorAllocator.OnInit(poolCounts);

    // Create the default texture:
    auto &pool = mFrame.CurrentPool();
    auto imgData = ImageData::SinglePixel(Pixel{255, 255, 255, 255});

    mDefaultImage.TexImage =
        ImageLoaders::LoadImage2D(mCtx, mQueues.Graphics, pool, imgData);

    auto format = mDefaultImage.TexImage.Info.Format;
    mDefaultImage.View = Image::CreateView2D(mCtx, mDefaultImage.TexImage, format,
                                             VK_IMAGE_ASPECT_COLOR_BIT);

    mMainDeletionQueue.push_back(mDefaultImage.TexImage);
    mMainDeletionQueue.push_back(mDefaultImage.View);

    // Create the texture sampler:
    mSampler = SamplerBuilder()
                   .SetMagFilter(VK_FILTER_LINEAR)
                   .SetMinFilter(VK_FILTER_LINEAR)
                   .SetAddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT)
                   .Build(mCtx);

    mMainDeletionQueue.push_back(mSampler);

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
        PipelineBuilder()
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
            .Build(mCtx);

    mPipelineDeletionQueue.push_back(mColoredPipeline.Handle);
    mPipelineDeletionQueue.push_back(mColoredPipeline.Layout);

    auto textuedShaderStages =
        ShaderBuilder()
            .SetVertexPath("assets/spirv/Minimal3DTexturedVert.spv")
            .SetFragmentPath("assets/spirv/Minimal3DTexturedFrag.spv")
            .Build(mCtx);

    mTexturedPipeline =
        PipelineBuilder()
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
            .Build(mCtx);

    mPipelineDeletionQueue.push_back(mTexturedPipeline.Handle);
    mPipelineDeletionQueue.push_back(mTexturedPipeline.Layout);
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

        for (auto &[_, mesh] : mColoredMeshes)
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
                                         mColoredLayout.IndexType);

                    vkCmdPushConstants(cmd, mColoredPipeline.Layout,
                                       VK_SHADER_STAGE_ALL_GRAPHICS, 0, sizeof(transform),
                                       &transform);
                    vkCmdDrawIndexed(cmd, drawable.IndexCount, 1, 0, 0, 0);
                }
            }
        }

        // 2. Textured vertices pass:
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mTexturedPipeline.Handle);

        common::ViewportScissor(cmd, GetTargetSize());

        // To-do: figure out a better way of doing this:
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                mTexturedPipeline.Layout, 0, 1, mCamera->DescriptorSet(),
                                0, nullptr);

        for (auto &[_, mesh] : mTexturedMeshes)
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
                                         mTexturedLayout.IndexType);

                    // To-do: currently descriptor for the material is bound each draw
                    // In reality draws should be sorted according to the material
                    // and corresponding descriptors only re-bound when change
                    // is necessary.
                    // auto &texture = mTextures[drawable.TextureId];
                    auto &material = mMaterials.at(drawable.Material);

                    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                            mTexturedPipeline.Layout, 1, 1,
                                            &material.DescriptorSet, 0, nullptr);

                    PushConstantData pcData{glm::vec4(material.AlphaCutoff), transform};

                    vkCmdPushConstants(cmd, mTexturedPipeline.Layout,
                                       VK_SHADER_STAGE_ALL_GRAPHICS, 0, sizeof(pcData),
                                       &pcData);
                    vkCmdDrawIndexed(cmd, drawable.IndexCount, 1, 0, 0, 0);
                }
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

    // Create depth buffer:
    ImageInfo depthBufferInfo{
        .Extent = drawExtent,
        .Format = mDepthFormat,
        .Tiling = VK_IMAGE_TILING_OPTIMAL,
        .Usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .MipLevels = 1,
    };

    mDepthBuffer = Image::CreateImage2D(mCtx, depthBufferInfo);
    mDepthBufferView =
        Image::CreateView2D(mCtx, mDepthBuffer, mDepthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

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

        mSceneDeletionQueue.push_back(drawable.VertexBuffer);
        mSceneDeletionQueue.push_back(drawable.IndexBuffer);
    };

    for (const auto &[key, sceneMesh] : scene.Meshes)
    {
        if (mColoredMeshes.count(key) != 0)
            continue;

        if (mTexturedMeshes.count(key) != 0)
            continue;

        if (mColoredLayout.IsCompatible(sceneMesh.Layout))
        {
            auto &mesh = mColoredMeshes[key];

            for (auto &geo : sceneMesh.Geometry)
            {
                auto &drawable = mesh.Drawables.emplace_back();
                CreateBuffers(drawable, geo);
            }
        }

        if (mTexturedLayout.IsCompatible(sceneMesh.Layout))
        {
            auto &mesh = mTexturedMeshes[key];

            for (auto &geo : sceneMesh.Geometry)
            {
                auto &drawable = mesh.Drawables.emplace_back();
                CreateBuffers(drawable, geo);
            }
        }
    }
}

void Minimal3DRenderer::LoadImages(const Scene &scene)
{
    auto &pool = mFrame.CurrentPool();

    for (auto &[key, imgData] : scene.Images)
    {
        if (mImages.count(key) != 0)
            continue;

        auto &texture = mImages[key];

        texture.TexImage =
            ImageLoaders::LoadImage2D(mCtx, mQueues.Graphics, pool, imgData);

        auto format = texture.TexImage.Info.Format;
        texture.View = Image::CreateView2D(mCtx, texture.TexImage, format,
                                           VK_IMAGE_ASPECT_COLOR_BIT);

        mSceneDeletionQueue.push_back(texture.TexImage);
        mSceneDeletionQueue.push_back(texture.View);
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

    for (const auto &[key, sceneMesh] : scene.Meshes)
    {
        if (mTexturedMeshes.count(key) != 0)
        {
            auto &mesh = mTexturedMeshes[key];

            for (const auto [idx, drawable] : enumerate(mesh.Drawables))
            {
                drawable.Material = sceneMesh.Materials[idx];
            }
        }
    }
}

void Minimal3DRenderer::LoadObjects(const Scene &scene)
{
    for (auto &[_, mesh] : mColoredMeshes)
        mesh.Transforms.clear();

    for (auto &[_, mesh] : mTexturedMeshes)
        mesh.Transforms.clear();

    for (auto &[_, obj] : scene.Objects)
    {
        if (!obj.Mesh.has_value())
            continue;

        SceneKey meshKey = obj.Mesh.value();

        if (mColoredMeshes.count(meshKey) != 0)
        {
            auto &mesh = mColoredMeshes[meshKey];

            mesh.Transforms.push_back(obj.Transform);

            continue;
        }

        if (mTexturedMeshes.count(meshKey) != 0)
        {
            auto &mesh = mTexturedMeshes[meshKey];

            mesh.Transforms.push_back(obj.Transform);

            continue;
        }
    }
}
