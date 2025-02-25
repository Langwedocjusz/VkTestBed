#include "MinimalPBR.h"
#include "Barrier.h"
#include "Common.h"
#include "Descriptor.h"
#include "ImageLoaders.h"
#include "MeshBuffers.h"
#include "Pipeline.h"
#include "Renderer.h"
#include "Sampler.h"
#include "Scene.h"
#include "Shader.h"
#include "VkInit.h"

#include <glm/matrix.hpp>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <ranges>
#include <iostream>

MinimalPbrRenderer::MinimalPbrRenderer(VulkanContext &ctx, FrameInfo &info,
                                       RenderContext::Queues &queues,
                                       std::unique_ptr<Camera> &camera)
    : IRenderer(ctx, info, queues, camera), mMaterialDescriptorAllocator(ctx),
      mEnvHandler(ctx, info, queues), mSceneDeletionQueue(ctx),
      mMaterialDeletionQueue(ctx)
{
    // Create the texture sampler:
    mSampler2D = SamplerBuilder()
                     .SetMagFilter(VK_FILTER_LINEAR)
                     .SetMinFilter(VK_FILTER_LINEAR)
                     .SetAddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT)
                     .SetMipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR)
                     .SetMaxLod(12.0f)
                     .Build(mCtx);
    mMainDeletionQueue.push_back(mSampler2D);

    // Create descriptor set layout for sampling textures:
    mMaterialDescriptorSetLayout =
        DescriptorSetLayoutBuilder()
            .AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        VK_SHADER_STAGE_FRAGMENT_BIT)
            .AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        VK_SHADER_STAGE_FRAGMENT_BIT)
            .AddBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        VK_SHADER_STAGE_FRAGMENT_BIT)
            .Build(ctx);

    mMainDeletionQueue.push_back(mMaterialDescriptorSetLayout);

    // Initialize descriptor allocator for materials:
    {
        std::vector<VkDescriptorPoolSize> poolCounts{
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
        };

        mMaterialDescriptorAllocator.OnInit(poolCounts);
    }

    //Create the default textures:
    auto& pool = mFrame.CurrentPool();

    auto albedoData = ImageData::SinglePixel(Pixel{255, 255, 255, 0});
    auto roughnessData = ImageData::SinglePixel(Pixel{0, 255, 255, 0});
    auto normalData = ImageData::SinglePixel(Pixel{0, 0, 255, 0});

    mDefaultAlbedo.Img = ImageLoaders::LoadImage2D(mCtx, mQueues.Graphics, pool, albedoData);
    mDefaultRoughness.Img = ImageLoaders::LoadImage2D(mCtx, mQueues.Graphics, pool, roughnessData);
    mDefaultNormal.Img = ImageLoaders::LoadImage2D(mCtx, mQueues.Graphics, pool, normalData);

    mDefaultAlbedo.View = Image::CreateView2D(mCtx, mDefaultAlbedo.Img, mDefaultAlbedo.Img.Info.Format,
                                           VK_IMAGE_ASPECT_COLOR_BIT);
    mDefaultRoughness.View = Image::CreateView2D(mCtx, mDefaultRoughness.Img, mDefaultRoughness.Img.Info.Format,
                                           VK_IMAGE_ASPECT_COLOR_BIT);
    mDefaultNormal.View = Image::CreateView2D(mCtx, mDefaultNormal.Img, mDefaultNormal.Img.Info.Format,
                                           VK_IMAGE_ASPECT_COLOR_BIT);

    mMainDeletionQueue.push_back(mDefaultAlbedo.Img);
    mMainDeletionQueue.push_back(mDefaultAlbedo.View);
    mMainDeletionQueue.push_back(mDefaultRoughness.Img);
    mMainDeletionQueue.push_back(mDefaultRoughness.View);
    mMainDeletionQueue.push_back(mDefaultNormal.Img);
    mMainDeletionQueue.push_back(mDefaultNormal.View);

    // Build the graphics pipelines:
    RebuildPipelines();

    // Create swapchain resources:
    CreateSwapchainResources();
}

MinimalPbrRenderer::~MinimalPbrRenderer()
{
    mMaterialDescriptorAllocator.DestroyPools();
    mSceneDeletionQueue.flush();
    mMaterialDeletionQueue.flush();
    mSwapchainDeletionQueue.flush();
    mPipelineDeletionQueue.flush();
    mMainDeletionQueue.flush();
}

void MinimalPbrRenderer::RebuildPipelines()
{
    mPipelineDeletionQueue.flush();

    // Main PBR pipeline:
    auto mainShaderStages = ShaderBuilder()
                                .SetVertexPath("assets/spirv/MinimalPBRVert.spv")
                                .SetFragmentPath("assets/spirv/MinimalPBRFrag.spv")
                                .Build(mCtx);

    mMainPipeline =
        PipelineBuilder()
            .SetShaderStages(mainShaderStages)
            .SetVertexInput(mGeometryLayout.VertexLayout, 0, VK_VERTEX_INPUT_RATE_VERTEX)
            .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetPolygonMode(VK_POLYGON_MODE_FILL)
            .SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .SetColorFormat(mRenderTargetFormat)
            .SetPushConstantSize(sizeof(MaterialPCData))
            .AddDescriptorSetLayout(mCamera->DescriptorSetLayout())
            .AddDescriptorSetLayout(mMaterialDescriptorSetLayout)
            .AddDescriptorSetLayout(mEnvHandler.GetDescriptorLayout())
            .EnableDepthTest()
            .SetDepthFormat(mDepthFormat)
            .Build(mCtx);

    mPipelineDeletionQueue.push_back(mMainPipeline.Handle);
    mPipelineDeletionQueue.push_back(mMainPipeline.Layout);

    // Background rendering pipeline:
    auto backgroundShaderStages = ShaderBuilder()
                                      .SetVertexPath("assets/spirv/BackgroundVert.spv")
                                      .SetFragmentPath("assets/spirv/BackgroundFrag.spv")
                                      .Build(mCtx);

    // No vertex format, since we just hardcode the fullscreen triangle in
    // the vertex shader:
    mBackgroundPipeline =
        PipelineBuilder()
            .SetShaderStages(backgroundShaderStages)
            .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetPolygonMode(VK_POLYGON_MODE_FILL)
            .SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .SetColorFormat(mRenderTargetFormat)
            .SetPushConstantSize(sizeof(BackgroundPCData))
            .AddDescriptorSetLayout(mEnvHandler.GetDescriptorLayout())
            .EnableDepthTest(VK_COMPARE_OP_LESS_OR_EQUAL)
            .SetDepthFormat(mDepthFormat)
            .Build(mCtx);

    mPipelineDeletionQueue.push_back(mBackgroundPipeline.Handle);
    mPipelineDeletionQueue.push_back(mBackgroundPipeline.Layout);

    // Rebuild env handler pipelines as well:
    mEnvHandler.RebuildPipelines();
}

void MinimalPbrRenderer::OnUpdate([[maybe_unused]] float deltaTime)
{

}

void MinimalPbrRenderer::OnImGui()
{
}

void MinimalPbrRenderer::OnRender()
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
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mMainPipeline.Handle);
        common::ViewportScissor(cmd, GetTargetSize());

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                mMainPipeline.Layout, 0, 1, mCamera->DescriptorSet(), 0,
                                nullptr);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                mMainPipeline.Layout, 2, 1,
                                mEnvHandler.GetDescriptorSetPtr(), 0, nullptr);

        uint32_t numDraws = 0, numIdx = 0;

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

                    // To-do: currently descriptor for the texture is bound each draw
                    // In reality draws should be sorted according to the material
                    // and corresponding descriptors only re-bound when change
                    // is necessary.
                    auto &material = mMaterials.at(drawable.MaterialKey);

                    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                            mMainPipeline.Layout, 1, 1,
                                            &material.DescriptorSet, 0, nullptr);

                    MaterialPCData pcData{
                        .AlphaCutoff = material.AlphaCutoff,
                        .ViewPos = mCamera->GetPos(),
                        .Transform = transform,
                    };

                    vkCmdPushConstants(cmd, mMainPipeline.Layout,
                                       VK_SHADER_STAGE_ALL_GRAPHICS, 0, sizeof(pcData),
                                       &pcData);
                    vkCmdDrawIndexed(cmd, drawable.IndexCount, 1, 0, 0, 0);

                    numIdx += drawable.IndexCount;
                    numDraws++;
                }
            }
        }

        mFrame.Stats.NumDraws = numDraws;
        mFrame.Stats.NumTriangles = numIdx / 3;

        // Draw the background:
        if (mEnvHandler.HdriEnabled())
        {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    mBackgroundPipeline.Layout, 0, 1,
                                    mEnvHandler.GetDescriptorSetPtr(), 0, nullptr);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              mBackgroundPipeline.Handle);
            common::ViewportScissor(cmd, GetTargetSize());

            auto proj = mCamera->GetProj();
            auto view = mCamera->GetView();
            auto inv = glm::inverse(proj * view);

            BackgroundPCData pcData{};

            pcData.BottomLeft = inv * glm::vec4(-1.0f, 1.0f, 1.0f, 1.0f);
            pcData.BottomRight = inv * glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
            pcData.TopLeft = inv * glm::vec4(-1.0f, -1.0f, 1.0f, 1.0f);
            pcData.TopRight = inv * glm::vec4(1.0f, -1.0f, 1.0f, 1.0f);

            vkCmdPushConstants(cmd, mBackgroundPipeline.Layout,
                               VK_SHADER_STAGE_ALL_GRAPHICS, 0, sizeof(pcData), &pcData);

            vkCmdDraw(cmd, 3, 1, 0, 0);
        }
    }
    vkCmdEndRendering(cmd);
}

void MinimalPbrRenderer::CreateSwapchainResources()
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

void MinimalPbrRenderer::LoadScene(const Scene &scene)
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

    if (scene.UpdateEnvironment())
        mEnvHandler.LoadEnvironment(scene);
}

void MinimalPbrRenderer::LoadMeshes(const Scene &scene)
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

        // Update deletion queue:
        mSceneDeletionQueue.push_back(drawable.VertexBuffer);
        mSceneDeletionQueue.push_back(drawable.IndexBuffer);
    };

    for (const auto &[key, sceneMesh] : scene.Meshes)
    {
        if (mMeshes.count(key) != 0)
            continue;

        if (mGeometryLayout.IsCompatible(sceneMesh.Layout))
        {
            auto &mesh = mMeshes[key];

            for (auto &geo : sceneMesh.Geometry)
            {
                auto &drawable = mesh.Drawables.emplace_back();

                CreateBuffers(drawable, geo);
            }
        }
    }
}

void MinimalPbrRenderer::LoadImages(const Scene &scene)
{
    auto &pool = mFrame.CurrentPool();

    for (auto &[key, imgData] : scene.Images)
    {
        if (mImages.count(key) != 0)
            continue;

        auto &texture = mImages[key];

        auto format = imgData.Unorm ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB;

        texture.Img =
            ImageLoaders::LoadImage2DMip(mCtx, mQueues.Graphics, pool, imgData, format);

        texture.View = Image::CreateView2D(mCtx, texture.Img, format,
                                           VK_IMAGE_ASPECT_COLOR_BIT);

        mSceneDeletionQueue.push_back(texture.Img);
        mSceneDeletionQueue.push_back(texture.View);
    }
}

void MinimalPbrRenderer::LoadMaterials(const Scene &scene)
{
    for (auto &[key, sceneMat] : scene.Materials)
    {
        const bool firstLoad = mMaterials.count(key) == 0;

        auto &mat = mMaterials[key];

        if (firstLoad)
        {
            mat.DescriptorSet =
                mMaterialDescriptorAllocator.Allocate(mMaterialDescriptorSetLayout);
        }

        // Update the alpha cutoff:
        mat.AlphaCutoff = sceneMat.AlphaCutoff;

        // Retrieve the textures if available:
        auto GetTexture = [&](std::optional<SceneKey> opt, Texture& def) -> Texture&
        {
            if (opt.has_value())
            {
                if (mImages.count(*opt) != 0)
                    return mImages[*opt];
            }

            return def;
        };

        auto &albedo = GetTexture(sceneMat.Albedo, mDefaultAlbedo);
        auto &roughness = GetTexture(sceneMat.Roughness, mDefaultRoughness);
        auto &normal = GetTexture(sceneMat.Normal, mDefaultNormal);

        // Update the descriptor set:
        DescriptorUpdater(mat.DescriptorSet)
            .WriteImageSampler(0, albedo.View, mSampler2D)
            .WriteImageSampler(1, roughness.View, mSampler2D)
            .WriteImageSampler(2, normal.View, mSampler2D)
            .Update(mCtx);
    }
}

void MinimalPbrRenderer::LoadMeshMaterials(const Scene &scene)
{
    using namespace std::views;

    for (const auto &[key, sceneMesh] : scene.Meshes)
    {
        if (mMeshes.count(key) != 0)
        {
            auto &mesh = mMeshes[key];

            for (const auto [idx, drawable] : enumerate(mesh.Drawables))
            {
                drawable.MaterialKey = sceneMesh.Materials[idx];
            }
        }
    }
}

void MinimalPbrRenderer::LoadObjects(const Scene &scene)
{
    for (auto &[_, mesh] : mMeshes)
        mesh.Transforms.clear();

    for (auto &[_, obj] : scene.Objects)
    {
        if (!obj.Mesh.has_value())
            continue;

        SceneKey meshKey = obj.Mesh.value();

        if (mMeshes.count(meshKey) != 0)
        {
            auto &mesh = mMeshes[meshKey];

            mesh.Transforms.push_back(obj.Transform);

            continue;
        }
    }
}