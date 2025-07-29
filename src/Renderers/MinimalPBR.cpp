#include "MinimalPBR.h"
#include "Camera.h"
#include "Pch.h"

#include "Barrier.h"
#include "BufferUtils.h"
#include "Common.h"
#include "Descriptor.h"
#include "ImageLoaders.h"
#include "ImageUtils.h"
#include "Pipeline.h"
#include "Renderer.h"
#include "Sampler.h"
#include "Scene.h"
#include "VkInit.h"

#include <glm/matrix.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <imgui.h>

#include <imgui_impl_vulkan.h>
#include <limits>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <ranges>
#include <vulkan/vulkan_core.h>

MinimalPbrRenderer::MinimalPbrRenderer(VulkanContext &ctx, FrameInfo &info,
                                       Camera &camera)
    : IRenderer(ctx, info, camera), mShadowmapDescriptorAllocator(ctx), mMaterialDescriptorAllocator(ctx),
      mDynamicUBO(ctx, info), mEnvHandler(ctx), mSceneDeletionQueue(ctx),
      mMaterialDeletionQueue(ctx)
{
    // Create the texture samplers:
    mSampler2D = SamplerBuilder("MinimalPbrSampler2D")
                     .SetMagFilter(VK_FILTER_LINEAR)
                     .SetMinFilter(VK_FILTER_LINEAR)
                     .SetAddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT)
                     .SetMipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR)
                     .SetMaxLod(12.0f)
                     .Build(mCtx, mMainDeletionQueue);


    mSamplerShadowmap = SamplerBuilder("MinimalPbrSamplerShadowmap")
                            .SetMagFilter(VK_FILTER_NEAREST)
                            .SetMinFilter(VK_FILTER_NEAREST)
                            .SetAddressMode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER)
                            .SetBorderColor(VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE)
                            .Build(mCtx, mMainDeletionQueue);

    // Create descriptor set layout for sampling the shadowmap 
    // and allocate the corresponding descriptor set:

    mShadowmapDescriptorSetLayout = 
        DescriptorSetLayoutBuilder("MinimalPBRShadowmapDescriptorLayout")
            .AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .Build(ctx, mMainDeletionQueue);

    {
        std::vector<VkDescriptorPoolSize> poolCounts{
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
        };

        mShadowmapDescriptorAllocator.OnInit(poolCounts);

        mShadowmapDescriptorSet = 
            mShadowmapDescriptorAllocator.Allocate(mShadowmapDescriptorSetLayout);
    }

    // Create descriptor set layout for sampling material textures:
    mMaterialDescriptorSetLayout =
        DescriptorSetLayoutBuilder("MinimalPBRMaterialDescriptorLayout")
            .AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        VK_SHADER_STAGE_FRAGMENT_BIT)
            .AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        VK_SHADER_STAGE_FRAGMENT_BIT)
            .AddBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        VK_SHADER_STAGE_FRAGMENT_BIT)
            .Build(ctx, mMainDeletionQueue);

    // Initialize descriptor allocator for materials:
    {
        std::vector<VkDescriptorPoolSize> poolCounts{
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
        };

        mMaterialDescriptorAllocator.OnInit(poolCounts);
    }

    // Create the default textures:
    auto albedoData = ImageData::SinglePixel(Pixel{255, 255, 255, 0}, false);
    auto roughnessData = ImageData::SinglePixel(Pixel{0, 255, 255, 0}, true);
    auto normalData = ImageData::SinglePixel(Pixel{128, 128, 255, 0}, true);

    mDefaultAlbedo = TextureLoaders::LoadTexture2D(mCtx, "DefaultAlbedo", albedoData,
                                                   VK_FORMAT_R8G8B8A8_SRGB);
    mDefaultRoughness = TextureLoaders::LoadTexture2D(
        mCtx, "DefaultRoughness", roughnessData, VK_FORMAT_R8G8B8A8_UNORM);
    mDefaultNormal = TextureLoaders::LoadTexture2D(mCtx, "DefaultNormal", normalData,
                                                   VK_FORMAT_R8G8B8A8_UNORM);

    mMainDeletionQueue.push_back(mDefaultAlbedo);
    mMainDeletionQueue.push_back(mDefaultRoughness);
    mMainDeletionQueue.push_back(mDefaultNormal);

    // Create the shadowmap:
    const uint32_t shadowRes = 2048;

    Image2DInfo shadowmapInfo{
        .Extent = {shadowRes, shadowRes},
        .Format = mDepthFormat,
        .Tiling = VK_IMAGE_TILING_OPTIMAL,
        .Usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .MipLevels = 1,
        .Layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
    };

    mShadowmap = MakeImage::Image2D(mCtx, "Shadowmap", shadowmapInfo);
    mShadowmapView = MakeView::View2D(mCtx, "ShadowmapView", mShadowmap, mDepthFormat,
                                      VK_IMAGE_ASPECT_DEPTH_BIT);

    mMainDeletionQueue.push_back(mShadowmap);
    mMainDeletionQueue.push_back(mShadowmapView);

    //Update the shadowmap descriptor:
    DescriptorUpdater(mShadowmapDescriptorSet)
        .WriteImageSampler(0, mShadowmapView, mSamplerShadowmap)
        .Update(mCtx);

    //Build dynamic uniform buffers & descriptors:
    auto stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    mDynamicUBO.OnInit("MinimalPBRDynamicUBO", stages, sizeof(mUBOData));

    // Build the graphics pipelines:
    RebuildPipelines();

    // Create swapchain resources:
    CreateSwapchainResources();

    mDebugTextureDescriptorSet = ImGui_ImplVulkan_AddTexture(mSampler2D, mShadowmapView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

MinimalPbrRenderer::~MinimalPbrRenderer()
{
    mMaterialDescriptorAllocator.DestroyPools();
    mShadowmapDescriptorAllocator.DestroyPools();
    mSceneDeletionQueue.flush();
    mMaterialDeletionQueue.flush();
    mSwapchainDeletionQueue.flush();
    mPipelineDeletionQueue.flush();
    mMainDeletionQueue.flush();
}

void MinimalPbrRenderer::RebuildPipelines()
{
    mPipelineDeletionQueue.flush();

    mShadowmapPipeline =
        PipelineBuilder("MinimalPBRShadowmapPipeline")
            .SetShaderPathVertex("assets/spirv/ShadowmapVert.spv")
            .SetShaderPathFragment("assets/spirv/ShadowmapFrag.spv")
            .SetVertexInput(mGeometryLayout.VertexLayout, 0, VK_VERTEX_INPUT_RATE_VERTEX)
            .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetPolygonMode(VK_POLYGON_MODE_FILL)
            .SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE)
            .RequestDynamicState(VK_DYNAMIC_STATE_CULL_MODE)
            .SetPushConstantSize(sizeof(MaterialPCData))
            .AddDescriptorSetLayout(mDynamicUBO.DescriptorSetLayout())
            .AddDescriptorSetLayout(mMaterialDescriptorSetLayout)
            .EnableDepthTest()
            .SetDepthFormat(mDepthFormat)
            .Build(mCtx, mPipelineDeletionQueue);

    mMainPipeline =
        PipelineBuilder("MinimalPBRMainPipeline")
            .SetShaderPathVertex("assets/spirv/MinimalPBRVert.spv")
            .SetShaderPathFragment("assets/spirv/MinimalPBRFrag.spv")
            .SetVertexInput(mGeometryLayout.VertexLayout, 0, VK_VERTEX_INPUT_RATE_VERTEX)
            .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetPolygonMode(VK_POLYGON_MODE_FILL)
            .SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .RequestDynamicState(VK_DYNAMIC_STATE_CULL_MODE)
            .SetColorFormat(mRenderTargetFormat)
            .SetPushConstantSize(sizeof(MaterialPCData))
            .AddDescriptorSetLayout(mDynamicUBO.DescriptorSetLayout())
            .AddDescriptorSetLayout(mMaterialDescriptorSetLayout)
            .AddDescriptorSetLayout(mEnvHandler.GetLightingDSLayout())
            .AddDescriptorSetLayout(mShadowmapDescriptorSetLayout)
            .EnableDepthTest()
            .SetDepthFormat(mDepthFormat)
            .Build(mCtx, mPipelineDeletionQueue);

    mBackgroundPipeline =
        PipelineBuilder("MinimalPBRBackgroundPipeline")
            .SetShaderPathVertex("assets/spirv/BackgroundVert.spv")
            .SetShaderPathFragment("assets/spirv/BackgroundFrag.spv")
            // No vertex format, since we just hardcode the fullscreen triangle in
            // the vertex shader:
            .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetPolygonMode(VK_POLYGON_MODE_FILL)
            .SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .SetColorFormat(mRenderTargetFormat)
            .SetPushConstantSize(sizeof(FrustumData))
            .AddDescriptorSetLayout(mEnvHandler.GetBackgroundDSLayout())
            .EnableDepthTest(VK_COMPARE_OP_LESS_OR_EQUAL)
            .SetDepthFormat(mDepthFormat)
            .Build(mCtx, mPipelineDeletionQueue);

    // Rebuild env handler pipelines as well:
    mEnvHandler.RebuildPipelines();
}

void MinimalPbrRenderer::OnUpdate([[maybe_unused]] float deltaTime)
{
    auto& camFr = mCamera.GetFrustum();

    mFrustumData.BottomLeft = camFr.FarBottomLeft;
    mFrustumData.BottomRight = camFr.FarBottomRight;
    mFrustumData.TopLeft = camFr.FarTopLeft;
    mFrustumData.TopRight = camFr.FarTopRight;


    //Construct worldspace coords for the shadow sampling part of the frustum:
    auto GetFarVector = [&](glm::vec4 near, glm::vec4 far)
    {
        auto normalized = glm::normalize(glm::vec3(far - near));
        return near + glm::vec4(mShadowDist * normalized, 0.0f);
    };

    Frustum shadowedFrustum = camFr;
    shadowedFrustum.FarTopLeft = GetFarVector(camFr.NearTopLeft, camFr.FarTopLeft);
    shadowedFrustum.FarTopRight = GetFarVector(camFr.NearTopRight, camFr.FarTopRight);
    shadowedFrustum.FarBottomLeft = GetFarVector(camFr.NearBottomLeft, camFr.FarBottomLeft);
    shadowedFrustum.FarBottomRight = GetFarVector(camFr.NearBottomRight, camFr.FarBottomRight);

    //Construct light view matrix, looking along the light direction:
    auto lightDir = mEnvHandler.GetUboData().LightDir;

    glm::mat4 lightView = glm::lookAt(lightDir, glm::vec3(0.0f), glm::vec3(0, -1, 0));

    //Transform the shadowed frustum to light view space:
    auto frustumVertices = shadowedFrustum.GetVertices();

    for (auto& vert : frustumVertices)
    {
        vert = lightView * vert;
    }

    //Find the extents of the frustum in light view space to get ortho projection bounds:
    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float minZ = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::min();
    float maxY = std::numeric_limits<float>::min();
    float maxZ = std::numeric_limits<float>::min();

    for (auto& vert : frustumVertices)
    {
        minX = std::min(minX, vert.x);
        minY = std::min(minY, vert.y);
        minZ = std::min(minZ, vert.z);
        maxX = std::max(maxX, vert.x);
        maxY = std::max(maxY, vert.y);
        maxZ = std::max(maxZ, vert.z);
    }

    //Adjust with user controlled parameters:
    maxZ += mAddZ;
    minZ -= mSubZ;

    glm::mat4 lightProj = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);   

    //Update light/camera uniform buffer data:
    mUBOData.CameraViewProjection = mCamera.GetViewProj();
    mUBOData.LightViewProjection = lightProj * lightView;
}

void MinimalPbrRenderer::OnImGui()
{
    ImGui::Begin("Renderer settings");

    ImGui::SliderFloat("Directional Factor", &mUBOData.DirectionalFactor, 0.0f, 6.0f);
    ImGui::SliderFloat("Environment Factor", &mUBOData.EnvironmentFactor, 0.0f, 1.0f);

    ImGui::SliderFloat("Add Z", &mAddZ, 0.0f, 10.0f);
    ImGui::SliderFloat("Sub Z", &mSubZ, 0.0f, 20.0f);
    ImGui::SliderFloat("Shadow Dist", &mShadowDist, 1.0f, 40.0f);
    ImGui::SliderFloat("Shadow Bias Min", &mUBOData.ShadowBiasMin, 0.0f, 0.1f);
    ImGui::SliderFloat("Shadow Bias Max", &mUBOData.ShadowBiasMax, 0.0f, 0.1f);

    ImGui::Image((ImTextureID)mDebugTextureDescriptorSet, ImVec2(512, 512));

    ImGui::End();
}

void MinimalPbrRenderer::OnRender()
{
    auto &cmd = mFrame.CurrentCmd();

    //This is not OnUpdate since, uniform buffers are per-image index
    //and as such need to be acquired after new image index is set.
    mDynamicUBO.UpdateData(&mUBOData, sizeof(mUBOData));

    DrawStats stats{};

    ShadowPass(cmd, stats);
    MainPass(cmd, stats);

    mFrame.Stats.NumTriangles = stats.NumIdx / 3;
    mFrame.Stats.NumDraws = stats.NumDraws;
    mFrame.Stats.NumBinds = stats.NumBinds;
}

void MinimalPbrRenderer::Draw(VkCommandBuffer cmd, Drawable &drawable, bool doubleSided, VkPipelineLayout layout, DrawStats &stats)
{
    VkBuffer vertBuffer = drawable.VertexBuffer.Handle;
    VkDeviceSize vertOffset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertBuffer, &vertOffset);

    vkCmdBindIndexBuffer(cmd, drawable.IndexBuffer.Handle, 0, mGeometryLayout.IndexType);

    auto &material = mMaterials.at(drawable.MaterialKey);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 1,
                            1, &material.DescriptorSet, 0, nullptr);

    for (auto &transform : drawable.Instances)
    {
        MaterialPCData pcData{
            .Transform = transform,
            .DoubleSided = doubleSided,
            .AlphaCutoff = material.AlphaCutoff,
        };

        vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_ALL_GRAPHICS, 0,
                           sizeof(pcData), &pcData);
        vkCmdDrawIndexed(cmd, drawable.IndexCount, 1, 0, 0, 0);

        stats.NumDraws++;
        stats.NumIdx += drawable.IndexCount;
    }

    stats.NumBinds += 3;
}

void MinimalPbrRenderer::ShadowPass(VkCommandBuffer cmd, DrawStats &stats)
{
    auto extent = VkExtent2D(mShadowmap.Info.extent.width, mShadowmap.Info.extent.height);

    auto barrierInfo = barrier::ImageLayoutBarrierInfo{
        .Image = mShadowmap.Handle,
        .OldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .NewLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .SubresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1},
    };
    barrier::ImageLayoutBarrierCoarse(cmd, barrierInfo);

    VkClearValue depthClear;
    depthClear.depthStencil = {1.0f, 0};

    auto depthAttachment = vkinit::CreateAttachmentInfo(
        mShadowmapView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, depthClear);

    VkRenderingInfo renderingInfo =
        vkinit::CreateRenderingInfoDepthOnly(extent, depthAttachment);

    vkCmdBeginRendering(cmd, &renderingInfo);
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mShadowmapPipeline.Handle);
        common::ViewportScissor(cmd, extent);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                mShadowmapPipeline.Layout, 0, 1, mDynamicUBO.DescriptorSet(),
                                0, nullptr);

        stats.NumBinds += 1;

        // Draw single-sided drawables:
        vkCmdSetCullMode(cmd, VK_CULL_MODE_BACK_BIT);

        for (auto key : mSingleSidedDrawableKeys)
        {
            Draw(cmd, mDrawables[key], false, mShadowmapPipeline.Layout, stats);
        }

        // Draw double-sided drawables:
        vkCmdSetCullMode(cmd, VK_CULL_MODE_NONE);

        for (auto key : mDoubleSidedDrawableKeys)
        {
            Draw(cmd, mDrawables[key], true, mShadowmapPipeline.Layout, stats);
        }
    }
    vkCmdEndRendering(cmd);

    barrierInfo.OldLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    barrierInfo.NewLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier::ImageLayoutBarrierCoarse(cmd, barrierInfo);
}

void MinimalPbrRenderer::MainPass(VkCommandBuffer cmd, DrawStats &stats)
{
    barrier::ImageBarrierDepthToRender(cmd, mDepthBuffer.Handle);

    VkClearValue clear;
    clear.color = {{0.0f, 0.0f, 0.0f, 0.0f}};

    auto colorAttachment = vkinit::CreateAttachmentInfo(
        mRenderTargetView, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, clear);

    VkClearValue depthClear;
    depthClear.depthStencil = {1.0f, 0};

    auto depthAttachment = vkinit::CreateAttachmentInfo(
        mDepthBufferView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, depthClear);

    VkRenderingInfo renderingInfo =
        vkinit::CreateRenderingInfo(GetTargetSize(), colorAttachment, depthAttachment);

    vkCmdBeginRendering(cmd, &renderingInfo);
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mMainPipeline.Handle);
        common::ViewportScissor(cmd, GetTargetSize());

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                mMainPipeline.Layout, 0, 1, mDynamicUBO.DescriptorSet(),
                                0, nullptr);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                mMainPipeline.Layout, 2, 1,
                                mEnvHandler.GetLightingDSPtr(), 0, nullptr);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                mMainPipeline.Layout, 3, 1,
                                &mShadowmapDescriptorSet, 0, nullptr);

        stats.NumBinds += 2;

        // Draw single-sided drawables:
        vkCmdSetCullMode(cmd, VK_CULL_MODE_BACK_BIT);

        for (auto key : mSingleSidedDrawableKeys)
        {
            Draw(cmd, mDrawables[key], false, mMainPipeline.Layout, stats);
        }

        // Draw double-sided drawables:
        vkCmdSetCullMode(cmd, VK_CULL_MODE_NONE);

        for (auto key : mDoubleSidedDrawableKeys)
        {
            Draw(cmd, mDrawables[key], true, mMainPipeline.Layout, stats);
        }

        // Draw the background:
        if (mEnvHandler.HdriEnabled())
        {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    mBackgroundPipeline.Layout, 0, 1,
                                    mEnvHandler.GetBackgroundDSPtr(), 0, nullptr);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              mBackgroundPipeline.Handle);
            common::ViewportScissor(cmd, GetTargetSize());

            vkCmdPushConstants(cmd, mBackgroundPipeline.Layout,
                               VK_SHADER_STAGE_ALL_GRAPHICS, 0, sizeof(mFrustumData), &mFrustumData);

            vkCmdDraw(cmd, 3, 1, 0, 0);

            stats.NumBinds += 2;
            stats.NumDraws += 1;
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

void MinimalPbrRenderer::LoadScene(const Scene &scene)
{
    // To-do: OWNERSHIP!!!!!!
    if (scene.FullReload())
    {
        mDrawables.clear();
        mMaterials.clear();
        mImages.clear();
    }

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
            }
        }
    }
}

void MinimalPbrRenderer::LoadImages(const Scene &scene)
{
    for (auto &[key, imgData] : scene.Images)
    {
        if (mImages.count(key) != 0)
            continue;

        auto &texture = mImages[key];

        auto format = imgData.Unorm ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB;

        texture =
            TextureLoaders::LoadTexture2DMipped(mCtx, "MaterialTexture", imgData, format);

        mSceneDeletionQueue.push_back(texture);
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

        // Update the alpha cutoff and doublesidedness:
        mat.AlphaCutoff = sceneMat.AlphaCutoff;
        mat.DoubleSided = sceneMat.DoubleSided;

        // Retrieve the textures if available:
        auto GetTexture = [&](std::optional<SceneKey> opt, Texture &def) -> Texture & {
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

    mSingleSidedDrawableKeys.clear();
    mDoubleSidedDrawableKeys.clear();

    for (const auto &[meshKey, mesh] : scene.Meshes)
    {
        for (const auto [primIdx, prim] : enumerate(mesh.Primitives))
        {
            auto drawableKey = DrawableKey{meshKey, primIdx};

            if (mDrawables.count(drawableKey) != 0)
            {
                auto matKey = *prim.Material;
                auto &mat = mMaterials[matKey];

                auto &drawable = mDrawables[drawableKey];

                if (prim.Material)
                    drawable.MaterialKey = matKey;

                if (mat.DoubleSided)
                    mDoubleSidedDrawableKeys.push_back(drawableKey);
                else
                    mSingleSidedDrawableKeys.push_back(drawableKey);
            }
        }
    }
}

void MinimalPbrRenderer::LoadObjects(const Scene &scene)
{
    using namespace std::views;

    for (auto &[_, drawable] : mDrawables)
        drawable.Instances.clear();

    for (const auto &[key, obj] : scene.Objects)
    {
        if (!obj.Mesh.has_value())
            continue;

        auto meshKey = *obj.Mesh;

        for (const auto [primIdx, _] : enumerate(scene.Meshes.at(meshKey).Primitives))
        {
            auto drawableKey = DrawableKey{meshKey, primIdx};
            mDrawables[drawableKey].Instances.push_back(obj.Transform);
        }
    }
}