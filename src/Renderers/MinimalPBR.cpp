#include "MinimalPBR.h"
#include "Barrier.h"
#include "Common.h"
#include "Descriptor.h"
#include "GeometryProvider.h"
#include "ImageData.h"
#include "ImageLoaders.h"
#include "Material.h"
#include "MeshBuffers.h"
#include "Pipeline.h"
#include "Renderer.h"
#include "Sampler.h"
#include "Scene.h"
#include "Shader.h"
#include "VkInit.h"
#include "VkUtils.h"
#include "glm/matrix.hpp"

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <ranges>
#include <variant>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

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

    // Create descriptor set layout for sampling textures
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

    // Initialize descriptor allocator for materials
    {
        std::vector<VkDescriptorPoolSize> poolCounts{
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
        };

        mMaterialDescriptorAllocator.OnInit(poolCounts);
    }

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
    //If queue empty, to nothing
    if (mMaterialData.Empty())
        return;

    //Otherwise wait till gpu is idle
    vkDeviceWaitIdle(mCtx.Device);

    //And attempt to create all queued materials
    auto &pool = mFrame.CurrentPool();

    auto LoadTexture = [&](Image& img, VkImageView& view, ImageData* data, bool unorm)
    {
        auto format = unorm ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB;

        img = ImageLoaders::LoadImage2DMip(mCtx, mQueues.Graphics, pool, data, format);
        view = Image::CreateView2D(mCtx, img, format, VK_IMAGE_ASPECT_COLOR_BIT);
    };

    while(auto opt = mMaterialData.TryPop())
    {
        auto& data = *opt;

        auto& mat = mMaterials[data.Key];

        LoadTexture(mat.AlbedoImage, mat.AlbedoView, data.Albedo.get(), false);
        LoadTexture(mat.RoughnessImage, mat.RoughnessView, data.Roughness.get(), true);
        LoadTexture(mat.NormalImage, mat.NormalView, data.Normal.get(), true);

        // Update the descriptor set:
        DescriptorUpdater(mat.DescriptorSet)
            .WriteImageSampler(0, mat.AlbedoView, mSampler2D)
            .WriteImageSampler(1, mat.RoughnessView, mSampler2D)
            .WriteImageSampler(2, mat.NormalView, mSampler2D)
            .Update(mCtx);

        // Append resources to deletion queue:
        mMaterialDeletionQueue.push_back(mat.AlbedoImage);
        mMaterialDeletionQueue.push_back(mat.AlbedoView);

        mMaterialDeletionQueue.push_back(mat.RoughnessImage);
        mMaterialDeletionQueue.push_back(mat.RoughnessView);

        mMaterialDeletionQueue.push_back(mat.NormalImage);
        mMaterialDeletionQueue.push_back(mat.NormalView);
    }
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

void MinimalPbrRenderer::LoadScene(Scene &scene)
{
    if (scene.UpdateGeometry())
    {
        // mSceneDeletionQueue.flush();
        // mMeshes.clear();

        LoadProviders(scene);
    }

    if (scene.UpdateMaterials())
    {
        // mMaterialDeletionQueue.flush();
        // mMaterials.clear();
        // mMaterialDescriptorAllocator.ResetPools();

        LoadTextures(scene);
    }

    if (scene.UpdateMeshMaterials())
        LoadMeshMaterials(scene);

    if (scene.UpdateInstances())
        LoadInstances(scene);

    if (scene.UpdateEnvironment())
        mEnvHandler.LoadEnvironment(scene);
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

    for (auto &[key, mesh] : scene.Meshes())
    {
        if (mGeometryLayout.IsCompatible(mesh.GeoProvider.Layout))
        {
            if (mMeshes.count(key) == 0)
            {
                auto &newMesh = mMeshes[key];

                auto geometries = mesh.GeoProvider.GetGeometry();

                for (auto &geo : geometries)
                {
                    auto &drawable = newMesh.Drawables.emplace_back();

                    CreateBuffers(drawable, geo);

                    mSceneDeletionQueue.push_back(drawable.VertexBuffer);
                    mSceneDeletionQueue.push_back(drawable.IndexBuffer);
                }
            }
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

    auto *src = &std::get<Material::ImageSource>(val);

    // Check if path to the texture is valid
    bool validPath = std::filesystem::is_regular_file(src->Path);

    if (!validPath)
    {
        std::cerr << "Invalid texture path: " << src->Path.string() << '\n';
        return nullptr;
    }

    // Return image source
    return src;
}

void MinimalPbrRenderer::LoadTextures(Scene &scene)
{
    // Iterate over all materials in scene
    for (auto &[key, mat] : scene.Materials())
    {
        // If material already imported, skip it
        if (mMaterials.count(key) != 0)
            continue;

        // Retrieve the sources
        auto albedoSrc = GetTextureSource(mat, ::Material::Albedo);

        //  Skip material if it has no albedo
        if (albedoSrc == nullptr)
            continue;

        auto roughnessSrc = GetTextureSource(mat, ::Material::Roughness);
        auto normalSrc = GetTextureSource(mat, ::Material::Normal);

        //Create new rendered-side material:
        auto &material = mMaterials[key];

        //Push the task to load images from disk to the thread pool
        //(they will be uploaded to vulkan in OnUpdate):
        auto RetrieveMaterialData = [this, key, albedoSrc, roughnessSrc, normalSrc]()
        {
            MaterialData data{};
            data.Key = key;

            data.Albedo = ImageData::ImportSTB(albedoSrc->Path.string());

            if (roughnessSrc)
                data.Roughness = ImageData::ImportSTB(roughnessSrc->Path.string());
            else
                data.Roughness = ImageData::SinglePixel(Pixel{0, 255, 255, 0});

            if (normalSrc)
                data.Normal = ImageData::ImportSTB(normalSrc->Path.string());
            else
                data.Normal = ImageData::SinglePixel(Pixel{0, 0, 255, 0});

            mMaterialData.Push(std::move(data));
        };

        mThreadPool.Push(RetrieveMaterialData);

        // Unpack alpha-cutoff if present:
        if (mat.count(::Material::AlphaCutoff))
        {
            auto &var = mat[::Material::AlphaCutoff];
            material.AlphaCutoff = std::get<float>(var);
        }

        // Allocate the descriptor set:
        material.DescriptorSet =
            mMaterialDescriptorAllocator.Allocate(mMaterialDescriptorSetLayout);
    }
}

void MinimalPbrRenderer::LoadMeshMaterials(Scene &scene)
{
    using namespace std::views;

    for (const auto &[key, mesh] : scene.Meshes())
    {
        if (mMeshes.count(key) != 0)
        {
            auto &ourMesh = mMeshes[key];

            for (const auto [idx, drawable] : enumerate(ourMesh.Drawables))
            {
                drawable.MaterialKey = mesh.MaterialIds[idx];
            }
        }
    }
}

void MinimalPbrRenderer::LoadInstances(Scene &scene)
{
    for (auto &[_, mesh] : mMeshes)
        mesh.Transforms.clear();

    for (auto &obj : scene.Objects)
    {
        if (!obj.has_value())
            continue;

        if (!obj->MeshId.has_value())
            continue;

        SceneKey meshKey = obj->MeshId.value();

        if (mMeshes.count(meshKey) != 0)
        {
            auto &mesh = mMeshes[meshKey];

            mesh.Transforms.push_back(obj->Transform);

            continue;
        }
    }
}