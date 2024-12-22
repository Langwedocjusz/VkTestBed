#include "MinimalPBR.h"
#include "Barrier.h"
#include "Common.h"
#include "Descriptor.h"
#include "GeometryProvider.h"
#include "Material.h"
#include "MeshBuffers.h"
#include "Renderer.h"
#include "Sampler.h"
#include "Shader.h"
#include "VkInit.h"

#include <cstdint>
#include <iostream>
#include <ranges>
#include <variant>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

MinimalPbrRenderer::MinimalPbrRenderer(VulkanContext &ctx, FrameInfo &info,
                                       RenderContext::Queues &queues,
                                       std::unique_ptr<Camera> &camera)
    : IRenderer(ctx, info, queues, camera), mTextureDescriptorAllocator(ctx),
      mMaterialDeletionQueue(ctx)
{
    // Create descriptor set layout for sampling textures
    mTextureDescriptorSetLayout =
        DescriptorSetLayoutBuilder()
            .AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        VK_SHADER_STAGE_FRAGMENT_BIT)
            .AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        VK_SHADER_STAGE_FRAGMENT_BIT)
            .AddBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        VK_SHADER_STAGE_FRAGMENT_BIT)
            .Build(ctx);

    mMainDeletionQueue.push_back(mTextureDescriptorSetLayout);

    // Initialize descriptor allocator for materials
    constexpr uint32_t imgPerMat = 3;

    std::vector<VkDescriptorPoolSize> poolCounts{
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imgPerMat},
    };

    mTextureDescriptorAllocator.OnInit(poolCounts);

    // Build the graphics pipelines:
    RebuildPipelines();

    // Create the texture sampler:
    mAlbedoSampler = SamplerBuilder()
                         .SetMagFilter(VK_FILTER_LINEAR)
                         .SetMinFilter(VK_FILTER_LINEAR)
                         .SetAddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT)
                         .Build(mCtx);

    mMainDeletionQueue.push_back(mAlbedoSampler);

    mRoughnessSampler = SamplerBuilder()
                            .SetMagFilter(VK_FILTER_LINEAR)
                            .SetMinFilter(VK_FILTER_LINEAR)
                            .SetAddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT)
                            .Build(mCtx);

    mMainDeletionQueue.push_back(mRoughnessSampler);

    mNormalSampler = SamplerBuilder()
                         .SetMagFilter(VK_FILTER_LINEAR)
                         .SetMinFilter(VK_FILTER_LINEAR)
                         .SetAddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT)
                         .Build(mCtx);

    mMainDeletionQueue.push_back(mNormalSampler);

    // Create swapchain resources:
    CreateSwapchainResources();
}

MinimalPbrRenderer::~MinimalPbrRenderer()
{
    mTextureDescriptorAllocator.DestroyPools();
    mSceneDeletionQueue.flush();
    mMaterialDeletionQueue.flush();
    mSwapchainDeletionQueue.flush();
    mPipelineDeletionQueue.flush();
    mMainDeletionQueue.flush();
}

void MinimalPbrRenderer::RebuildPipelines()
{
    mPipelineDeletionQueue.flush();

    auto textuedShaderStages = ShaderBuilder()
                                   .SetVertexPath("assets/spirv/MinimalPBRVert.spv")
                                   .SetFragmentPath("assets/spirv/MinimalPBRFrag.spv")
                                   .Build(mCtx);

    mPipeline =
        PipelineBuilder()
            .SetShaderStages(textuedShaderStages)
            .SetVertexInput(mGeometryLayout.VertexLayout, 0, VK_VERTEX_INPUT_RATE_VERTEX)
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

    mPipelineDeletionQueue.push_back(mPipeline.Handle);
    mPipelineDeletionQueue.push_back(mPipeline.Layout);
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
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline.Handle);

        common::ViewportScissor(cmd, GetTargetSize());

        // To-do: figure out a better way of doing this:
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline.Layout, 0,
                                1, mCamera->DescriptorSet(), 0, nullptr);

        for (auto &mesh : mMeshes)
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
                    auto &material = mMaterials.at(drawable.MaterialId);

                    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                            mPipeline.Layout, 1, 1,
                                            &material.DescriptorSet, 0, nullptr);

                    PushConstantData pcData{glm::vec4(material.AlphaCutoff), transform};

                    vkCmdPushConstants(cmd, mPipeline.Layout,
                                       VK_SHADER_STAGE_ALL_GRAPHICS, 0, sizeof(pcData),
                                       &pcData);
                    vkCmdDrawIndexed(cmd, drawable.IndexCount, 1, 0, 0, 0);
                }
            }
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
    };

    mRenderTarget = Image::CreateImage2D(mCtx, renderTargetInfo);
    mSwapchainDeletionQueue.push_back(mRenderTarget);

    // Create the render target view:
    mRenderTargetView = Image::CreateView2D(mCtx, mRenderTarget, mRenderTargetFormat,
                                            VK_IMAGE_ASPECT_COLOR_BIT);
    mSwapchainDeletionQueue.push_back(mRenderTargetView);

    // Create depth buffer:
    ImageInfo depthBufferInfo{.Extent = drawExtent,
                              .Format = mDepthFormat,
                              .Tiling = VK_IMAGE_TILING_OPTIMAL,
                              .Usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT};

    mDepthBuffer = Image::CreateImage2D(mCtx, depthBufferInfo);
    mDepthBufferView =
        Image::CreateView2D(mCtx, mDepthBuffer, mDepthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

    mSwapchainDeletionQueue.push_back(mDepthBuffer);
    mSwapchainDeletionQueue.push_back(mDepthBufferView);
}

void MinimalPbrRenderer::LoadScene(Scene &scene)
{
    if (scene.UpdateGeometry())
    {
        mSceneDeletionQueue.flush();
        mMeshes.clear();
        mIdMap.clear();

        LoadProviders(scene);
    }

    if (scene.UpdateMaterials())
    {
        mMaterialDeletionQueue.flush();
        mMaterials.clear();
        mTextureDescriptorAllocator.ResetPools();

        LoadTextures(scene);
    }

    if (scene.UpdateMeshMaterials())
        LoadMeshMaterials(scene);

    if (scene.UpdateInstances())
        LoadInstances(scene);
}

void MinimalPbrRenderer::LoadProviders(Scene &scene)
{
    using namespace std::views;

    auto &pool = mFrame.CurrentPool();

    auto CreateBuffers = [&](Drawable &drawable, GeometryData &geo) {
        // Create Vertex buffer:
        drawable.VertexBuffer =
            VertexBuffer::Create(mCtx, mQueues.Graphics, pool, geo.VertexData);
        drawable.VertexCount = static_cast<uint32_t>(geo.VertexData.Count);

        // Create Index buffer:
        drawable.IndexBuffer =
            IndexBuffer::Create(mCtx, mQueues.Graphics, pool, geo.IndexData);
        drawable.IndexCount = static_cast<uint32_t>(geo.IndexData.Count);
    };

    for (const auto [id, mesh] : enumerate(scene.Meshes))
    {
        if (mGeometryLayout.IsCompatible(mesh.GeoProvider.Layout))
        {
            auto &newMesh = mMeshes.emplace_back();
            mIdMap[id] = mMeshes.size() - 1;

            auto geometries = mesh.GeoProvider.GetGeometry();

            for (auto &geo : geometries)
            {
                auto &drawable = newMesh.Drawables.emplace_back();
                CreateBuffers(drawable, geo);
            }

            continue;
        }
    }

    for (auto &mesh : mMeshes)
    {
        for (auto &drawable : mesh.Drawables)
        {
            mSceneDeletionQueue.push_back(drawable.VertexBuffer);
            mSceneDeletionQueue.push_back(drawable.IndexBuffer);
        }
    }
}

static Material::ImageSource *GetTextureSource(Material &mat, const MaterialKey &key)
{
    // Check if material has specified component
    if (mat.count(key) == 0)
        return nullptr;

    // Check if it is represented by a texture
    auto &val = mat[key];

    if (!std::holds_alternative<Material::ImageSource>(val))
        return nullptr;

    // Return image source
    return &std::get<Material::ImageSource>(val);
}

void MinimalPbrRenderer::TextureFromPath(Image &img, VkImageView &view,
                                         ::Material::ImageSource *source)
{
    auto &pool = mFrame.CurrentPool();

    img = ImageLoaders::LoadImage2D(mCtx, mQueues.Graphics, pool, source->Path.string());

    auto format = img.Info.Format;
    view = Image::CreateView2D(mCtx, img, format, VK_IMAGE_ASPECT_COLOR_BIT);
}

void MinimalPbrRenderer::TextureFromPath(Image &img, VkImageView &view,
                                         ::Material::ImageSource *source,
                                         ::ImageLoaders::Image2DData &defaultData)
{
    auto &pool = mFrame.CurrentPool();

    if (source)
        img = ImageLoaders::LoadImage2D(mCtx, mQueues.Graphics, pool,
                                        source->Path.string());
    else
        img = ImageLoaders::Image2DFromData(mCtx, mQueues.Graphics, pool, defaultData);

    auto format = img.Info.Format;
    view = Image::CreateView2D(mCtx, img, format, VK_IMAGE_ASPECT_COLOR_BIT);
}

void MinimalPbrRenderer::LoadTextures(Scene &scene)
{
    // Create images and views, allocate descriptor sets:
    for (auto &mat : scene.Materials)
    {
        auto albedo = GetTextureSource(mat, ::Material::Albedo);

        if (albedo == nullptr)
            continue;

        auto &material = mMaterials.emplace_back();

        TextureFromPath(material.AlbedoImage, material.AlbedoView, albedo);

        auto roughness = GetTextureSource(mat, ::Material::Roughness);
        auto normal = GetTextureSource(mat, ::Material::Normal);

        ImageLoaders::Image2DData roughnessDefault{
            .Width = 1,
            .Height = 1,
            .Data = {ImageLoaders::Pixel{0, 255, 255, 0}},
        };

        ImageLoaders::Image2DData normalDefault{
            .Width = 1,
            .Height = 1,
            .Data = {ImageLoaders::Pixel{0, 255, 0, 0}},
        };

        (void)roughness; (void)normal;

        TextureFromPath(material.RoughnessImage, material.RoughnessView, roughness,
                        roughnessDefault);
        TextureFromPath(material.NormalImage, material.NormalView, normal, normalDefault);

        // Unpack alpha-cutoff if present:
        if (mat.count(::Material::AlphaCutoff))
        {
            auto &var = mat[::Material::AlphaCutoff];
            material.AlphaCutoff = std::get<float>(var);
        }

        // Allocate descriptor set for the texture:
        material.DescriptorSet =
            mTextureDescriptorAllocator.Allocate(mTextureDescriptorSetLayout);
    }

    // Update descriptor sets:
    for (const auto &material : mMaterials)
    {
        DescriptorUpdater(material.DescriptorSet)
            .WriteImage(0, material.AlbedoView, mAlbedoSampler)
            .WriteImage(1, material.RoughnessView, mRoughnessSampler)
            .WriteImage(2, material.NormalView, mNormalSampler)
            .Update(mCtx);
    }

    for (auto &material : mMaterials)
    {
        mMaterialDeletionQueue.push_back(material.AlbedoImage);
        mMaterialDeletionQueue.push_back(material.AlbedoView);

        mMaterialDeletionQueue.push_back(material.RoughnessImage);
        mMaterialDeletionQueue.push_back(material.RoughnessView);

        mMaterialDeletionQueue.push_back(material.NormalImage);
        mMaterialDeletionQueue.push_back(material.NormalView);
    }
}

void MinimalPbrRenderer::LoadMeshMaterials(Scene &scene)
{
    using namespace std::views;

    size_t meshId = 0;

    for (const auto &mesh : scene.Meshes)
    {
        if (mGeometryLayout.IsCompatible(mesh.GeoProvider.Layout))
        {
            auto &ourMesh = mMeshes[meshId];

            for (const auto [idx, drawable] : enumerate(ourMesh.Drawables))
            {
                drawable.MaterialId = mesh.MaterialIds[idx];
            }

            meshId++;
        }
    }
}

void MinimalPbrRenderer::LoadInstances(Scene &scene)
{
    for (auto &mesh : mMeshes)
        mesh.Transforms.clear();

    for (auto &obj : scene.Objects)
    {
        if (!obj.has_value())
            continue;

        if (!obj->MeshId.has_value())
            continue;

        size_t meshId = obj->MeshId.value();

        auto &sceneMesh = scene.Meshes[meshId];

        if (mGeometryLayout.IsCompatible(sceneMesh.GeoProvider.Layout))
        {
            size_t id = mIdMap[meshId];
            auto &mesh = mMeshes[id];

            mesh.Transforms.push_back(obj->Transform);

            continue;
        }
    }
}
