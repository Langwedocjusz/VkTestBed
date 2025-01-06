#include "MinimalPBR.h"
#include "Barrier.h"
#include "Common.h"
#include "Descriptor.h"
#include "GeometryProvider.h"
#include "ImageLoaders.h"
#include "Material.h"
#include "MeshBuffers.h"
#include "Pipeline.h"
#include "Renderer.h"
#include "Sampler.h"
#include "Shader.h"
#include "Utils.h"
#include "VkInit.h"
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
    : IRenderer(ctx, info, queues, camera), mMainDescriptorAllocator(ctx),
      mMaterialDescriptorAllocator(ctx), mSceneDeletionQueue(ctx),
      mMaterialDeletionQueue(ctx), mEnvironmentDeletionQueue(ctx)
{
    // Create the texture sampler:
    mSampler2D = SamplerBuilder()
                     .SetMagFilter(VK_FILTER_LINEAR)
                     .SetMinFilter(VK_FILTER_LINEAR)
                     .SetAddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT)
                     .Build(mCtx);

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

    // Descriptor layout for generating the cubemap:
    mCubeGenDescriptorSetLayout =
        DescriptorSetLayoutBuilder()
            .AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
            .AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        VK_SHADER_STAGE_COMPUTE_BIT)
            .Build(ctx);

    mMainDeletionQueue.push_back(mCubeGenDescriptorSetLayout);

    // Descriptor layout for sampling the cubemap:
    mEnvironmentDescriptorSetLayout =
        DescriptorSetLayoutBuilder()
            .AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        VK_SHADER_STAGE_FRAGMENT_BIT)
            .Build(ctx);

    mMainDeletionQueue.push_back(mEnvironmentDescriptorSetLayout);

    // Initialize main descriptor allocator
    {
        std::vector<VkDescriptorPoolSize> poolCounts{
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2},
        };

        mMainDescriptorAllocator.OnInit(poolCounts);
    }

    mEnvironmentDescriptorSet =
        mMainDescriptorAllocator.Allocate(mEnvironmentDescriptorSetLayout);

    mCubeGenDescriptorSet =
        mMainDescriptorAllocator.Allocate(mCubeGenDescriptorSetLayout);

    // Create cubemap image and view, update the descriptor:
    constexpr uint32_t cubeSize = 1024;

    auto cubemapInfo = ImageInfo{
        .Extent = {cubeSize, cubeSize, 1},
        .Format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .Tiling = VK_IMAGE_TILING_OPTIMAL,
        .Usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    };

    mCubemap = Image::CreateCubemap(mCtx, cubemapInfo);
    mMainDeletionQueue.push_back(mCubemap);

    mCubemapView = Image::CreateViewCube(mCtx, mCubemap, cubemapInfo.Format,
                                         VK_IMAGE_ASPECT_COLOR_BIT);
    mMainDeletionQueue.push_back(mCubemapView);

    DescriptorUpdater(mEnvironmentDescriptorSet)
        .WriteImageSampler(0, mCubemapView, mSampler2D)
        .Update(mCtx);

    // Build the graphics pipelines:
    RebuildPipelines();

    mMainDeletionQueue.push_back(mSampler2D);

    // Create swapchain resources:
    CreateSwapchainResources();
}

MinimalPbrRenderer::~MinimalPbrRenderer()
{
    mMaterialDescriptorAllocator.DestroyPools();
    mMainDescriptorAllocator.DestroyPools();
    mSceneDeletionQueue.flush();
    mMaterialDeletionQueue.flush();
    mEnvironmentDeletionQueue.flush();
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
            .AddDescriptorSetLayout(mEnvironmentDescriptorSetLayout)
            .EnableDepthTest(VK_COMPARE_OP_LESS_OR_EQUAL)
            .SetDepthFormat(mDepthFormat)
            .Build(mCtx);

    mPipelineDeletionQueue.push_back(mBackgroundPipeline.Handle);
    mPipelineDeletionQueue.push_back(mBackgroundPipeline.Layout);

    auto equiToCubeShaderStages =
        ShaderBuilder().SetComputePath("assets/spirv/EquiToCubeComp.spv").Build(mCtx);

    mEquiRectToCubePipeline = ComputePipelineBuilder()
                                  .SetShaderStage(equiToCubeShaderStages[0])
                                  .AddDescriptorSetLayout(mCubeGenDescriptorSetLayout)
                                  .SetPushConstantSize(sizeof(CubeGenPCData))
                                  .Build(mCtx);

    mPipelineDeletionQueue.push_back(mEquiRectToCubePipeline.Handle);
    mPipelineDeletionQueue.push_back(mEquiRectToCubePipeline.Layout);
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

        // To-do: figure out a better way of doing this:
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                mMainPipeline.Layout, 0, 1, mCamera->DescriptorSet(), 0,
                                nullptr);

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
                                            mMainPipeline.Layout, 1, 1,
                                            &material.DescriptorSet, 0, nullptr);

                    MaterialPCData pcData{glm::vec4(material.AlphaCutoff), transform};

                    vkCmdPushConstants(cmd, mMainPipeline.Layout,
                                       VK_SHADER_STAGE_ALL_GRAPHICS, 0, sizeof(pcData),
                                       &pcData);
                    vkCmdDrawIndexed(cmd, drawable.IndexCount, 1, 0, 0, 0);
                }
            }
        }

        // Draw the background:
        if (mEnvironment)
        {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    mBackgroundPipeline.Layout, 0, 1,
                                    &mEnvironmentDescriptorSet, 0, nullptr);

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
        mMaterialDescriptorAllocator.ResetPools();

        LoadTextures(scene);
    }

    if (scene.UpdateMeshMaterials())
        LoadMeshMaterials(scene);

    if (scene.UpdateInstances())
        LoadInstances(scene);

    if (scene.UpdateEnvironment())
        LoadEnvironment(scene);
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

    auto *src = &std::get<Material::ImageSource>(val);

    bool validPath = std::filesystem::is_regular_file(src->Path);

    if (!validPath)
    {
        std::cerr << "Invalid texture path: " << src->Path.string() << '\n';
        return nullptr;
    }

    // Return image source
    return src;
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
                                         ::ImageLoaders::Image2DData &defaultData,
                                         bool unorm)
{
    auto &pool = mFrame.CurrentPool();

    auto format = unorm ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB;

    if (source)
        img = ImageLoaders::LoadImage2D(mCtx, mQueues.Graphics, pool,
                                        source->Path.string(), format);
    else
        img = ImageLoaders::Image2DFromData(mCtx, mQueues.Graphics, pool, defaultData,
                                            format);

    // auto format = img.Info.Format;
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

        TextureFromPath(material.RoughnessImage, material.RoughnessView, roughness,
                        roughnessDefault);
        TextureFromPath(material.NormalImage, material.NormalView, normal, normalDefault,
                        true);

        // Unpack alpha-cutoff if present:
        if (mat.count(::Material::AlphaCutoff))
        {
            auto &var = mat[::Material::AlphaCutoff];
            material.AlphaCutoff = std::get<float>(var);
        }

        // Allocate descriptor set for the texture:
        material.DescriptorSet =
            mMaterialDescriptorAllocator.Allocate(mMaterialDescriptorSetLayout);
    }

    // Update descriptor sets:
    for (const auto &material : mMaterials)
    {
        DescriptorUpdater(material.DescriptorSet)
            .WriteImageSampler(0, material.AlbedoView, mSampler2D)
            .WriteImageSampler(1, material.RoughnessView, mSampler2D)
            .WriteImageSampler(2, material.NormalView, mSampler2D)
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

void MinimalPbrRenderer::LoadEnvironment(Scene &scene)
{
    mEnvironmentDeletionQueue.flush();

    if (auto path = scene.HdriPath)
    {
        bool validPath = std::filesystem::is_regular_file(*path);

        if (!validPath)
        {
            std::cerr << "Invalid hdri path: " << (*path).string() << '\n';
            return;
        }

        auto &pool = mFrame.CurrentPool();

        // Load equirectangular environment map:
        auto envMap = ImageLoaders::LoadHDRI(mCtx, mQueues.Graphics, pool, *path);

        auto envView = Image::CreateView2D(mCtx, envMap, envMap.Info.Format,
                                           VK_IMAGE_ASPECT_COLOR_BIT);

        DescriptorUpdater(mCubeGenDescriptorSet)
            .WriteImageStorage(0, mCubemapView)
            .WriteImageSampler(1, envView, mSampler2D)
            .Update(mCtx);

        // Sample equirectangular map to cubemap
        // using a compute pipeline:
        {
            utils::ScopedCommand cmd(mCtx, mQueues.Graphics, pool);

            auto barrierInfo = barrier::ImageLayoutBarrierInfo{
                .Image = mCubemap.Handle,
                .OldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .NewLayout = VK_IMAGE_LAYOUT_GENERAL,
                .SubresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6},
            };
            barrier::ImageLayoutBarrierCoarse(cmd.Buffer, barrierInfo);

            vkCmdBindPipeline(cmd.Buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                              mEquiRectToCubePipeline.Handle);

            vkCmdBindDescriptorSets(cmd.Buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                    mEquiRectToCubePipeline.Layout, 0, 1,
                                    &mCubeGenDescriptorSet, 0, nullptr);

            for (int32_t side = 0; side < 6; side++)
            {
                auto pcData = CubeGenPCData{};
                pcData.SideId = side;

                vkCmdPushConstants(cmd.Buffer, mEquiRectToCubePipeline.Layout,
                                   VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pcData),
                                   &pcData);

                uint32_t localSizeX = 32, localSizeY = 32;

                uint32_t dispCountX = mCubemap.Info.Extent.width / localSizeX;
                uint32_t dispCountY = mCubemap.Info.Extent.width / localSizeY;

                vkCmdDispatch(cmd.Buffer, dispCountX, dispCountY, 1);
            }

            barrierInfo.OldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrierInfo.NewLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier::ImageLayoutBarrierCoarse(cmd.Buffer, barrierInfo);
        }

        // Clean up the equirectangular map:
        Image::DestroyImage(mCtx, envMap);
        vkDestroyImageView(mCtx.Device, envView, nullptr);

        // Update the rendering flag:
        mEnvironment = true;
    }
}