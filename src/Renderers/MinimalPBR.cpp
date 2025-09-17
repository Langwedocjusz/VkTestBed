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

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/matrix.hpp>

#include <imgui.h>
#include <imgui_impl_vulkan.h>

#include <utility>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <limits>
#include <ranges>
#include <vulkan/vulkan_core.h>

void MinimalPbrRenderer::DestroyDrawable(const MinimalPbrRenderer::Drawable &drawable)
{
    auto &vertexBuffer = drawable.VertexBuffer;
    auto &indexBuffer = drawable.IndexBuffer;

    vmaDestroyBuffer(mCtx.Allocator, vertexBuffer.Handle, vertexBuffer.Allocation);
    vmaDestroyBuffer(mCtx.Allocator, indexBuffer.Handle, indexBuffer.Allocation);
}

void MinimalPbrRenderer::DestroyTexture(const Texture &texture)
{
    vkDestroyImageView(mCtx.Device, texture.View, nullptr);
    vmaDestroyImage(mCtx.Allocator, texture.Img.Handle, texture.Img.Allocation);
}

MinimalPbrRenderer::MinimalPbrRenderer(VulkanContext &ctx, FrameInfo &info,
                                       Camera &camera)
    : IRenderer(ctx, info, camera), mMaterialDescriptorAllocator(ctx),
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
                            .SetMagFilter(VK_FILTER_LINEAR)
                            .SetMinFilter(VK_FILTER_LINEAR)
                            .SetAddressMode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER)
                            .SetBorderColor(VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE)
                            .SetCompareOp(VK_COMPARE_OP_LESS)
                            .Build(mCtx, mMainDeletionQueue);

    // Create static descriptor pool:
    {
        std::vector<VkDescriptorPoolSize> poolCounts{
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
        };

        mStaticDescriptorPool = Descriptor::InitPool(mCtx, 1, poolCounts);
        mMainDeletionQueue.push_back(mStaticDescriptorPool);
    }

    // Create descriptor set layout for sampling the shadowmap
    // and allocate the corresponding descriptor set:

    mShadowmapDescriptorSetLayout =
        DescriptorSetLayoutBuilder("MinimalPBRShadowmapDescriptorLayout")
            .AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        VK_SHADER_STAGE_FRAGMENT_BIT)
            .Build(ctx, mMainDeletionQueue);

    mShadowmapDescriptorSet =
        Descriptor::Allocate(mCtx, mStaticDescriptorPool, mShadowmapDescriptorSetLayout);

    // Create descriptor set layout for sampling material textures:
    mMaterialDescriptorSetLayout =
        DescriptorSetLayoutBuilder("MinimalPBRMaterialDescriptorLayout")
            .AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        VK_SHADER_STAGE_FRAGMENT_BIT)
            .AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        VK_SHADER_STAGE_FRAGMENT_BIT)
            .AddBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        VK_SHADER_STAGE_FRAGMENT_BIT)
            .AddBinding(3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                        VK_SHADER_STAGE_FRAGMENT_BIT)
            .Build(ctx, mMainDeletionQueue);

    // Initialize descriptor allocator for materials:
    {
        std::vector<VkDescriptorPoolSize> poolCounts{
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
        };

        mMaterialDescriptorAllocator.OnInit(poolCounts);
    }

    // Create the default textures:
    auto albedoData = ImageData::SinglePixel(Pixel{255, 255, 255, 255}, false);
    auto roughnessData = ImageData::SinglePixel(Pixel{0, 255, 255, 0}, true);
    auto normalData = ImageData::SinglePixel(Pixel{128, 128, 255, 0}, true);

    mDefaultAlbedo = TextureLoaders::LoadTexture2D(mCtx, "DefaultAlbedo", albedoData);
    mDefaultRoughness =
        TextureLoaders::LoadTexture2D(mCtx, "DefaultRoughness", roughnessData);
    mDefaultNormal = TextureLoaders::LoadTexture2D(mCtx, "DefaultNormal", normalData);

    mMainDeletionQueue.push_back(mDefaultAlbedo);
    mMainDeletionQueue.push_back(mDefaultRoughness);
    mMainDeletionQueue.push_back(mDefaultNormal);

    // Create the shadowmap:
    const uint32_t shadowRes = 2048;

    Image2DInfo shadowmapInfo{
        .Extent = {shadowRes, shadowRes},
        .Format = mShadowmapFormat,
        .Tiling = VK_IMAGE_TILING_OPTIMAL,
        .Usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .MipLevels = 1,
        .Layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    mShadowmap = MakeImage::Image2D(mCtx, "Shadowmap", shadowmapInfo);
    mShadowmapView = MakeView::View2D(mCtx, "ShadowmapView", mShadowmap, mShadowmapFormat,
                                      VK_IMAGE_ASPECT_DEPTH_BIT);

    mMainDeletionQueue.push_back(mShadowmap);
    mMainDeletionQueue.push_back(mShadowmapView);

    // Update the shadowmap descriptor:
    DescriptorUpdater(mShadowmapDescriptorSet)
        .WriteImageSampler(0, mShadowmapView, mSamplerShadowmap)
        .Update(mCtx);

    // Build dynamic uniform buffers & descriptors:
    auto stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    mDynamicUBO.OnInit("MinimalPBRDynamicUBO", stages, sizeof(mUBOData));

    // Build the graphics pipelines:
    RebuildPipelines();

    mDebugTextureDescriptorSet = ImGui_ImplVulkan_AddTexture(
        mSampler2D, mShadowmapView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

MinimalPbrRenderer::~MinimalPbrRenderer()
{
    mMaterialDescriptorAllocator.DestroyPools();

    for (auto &[_, drawable] : mDrawables)
        DestroyDrawable(drawable);

    for (auto &[_, img] : mImages)
        DestroyTexture(img);

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
            .SetPushConstantSize(sizeof(ShadowPCData))
            .AddDescriptorSetLayout(mDynamicUBO.DescriptorSetLayout())
            .AddDescriptorSetLayout(mMaterialDescriptorSetLayout)
            .EnableDepthTest()
            .SetDepthFormat(mShadowmapFormat)
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
            .SetPushConstantSize(sizeof(MainPCData))
            .AddDescriptorSetLayout(mDynamicUBO.DescriptorSetLayout())
            .AddDescriptorSetLayout(mMaterialDescriptorSetLayout)
            .AddDescriptorSetLayout(mEnvHandler.GetLightingDSLayout())
            .AddDescriptorSetLayout(mShadowmapDescriptorSetLayout)
            .EnableDepthTest()
            .SetDepthFormat(mDepthStencilFormat)
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
            .SetPushConstantSize(sizeof(FrustumBack))
            .AddDescriptorSetLayout(mEnvHandler.GetBackgroundDSLayout())
            .EnableDepthTest(VK_COMPARE_OP_LESS_OR_EQUAL)
            .SetDepthFormat(mDepthStencilFormat)
            .Build(mCtx, mPipelineDeletionQueue);

    // Rebuild env handler pipelines as well:
    mEnvHandler.RebuildPipelines();

    VkStencilOpState stencilWriteState{
        .failOp = VK_STENCIL_OP_REPLACE,
        .passOp = VK_STENCIL_OP_REPLACE,
        .depthFailOp = VK_STENCIL_OP_REPLACE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .compareMask = ~0u,
        .writeMask = ~0u,
        .reference = 1,
    };

    mStencilPipeline =
        PipelineBuilder("MinimalPBRStencilPipeline")
            .SetShaderPathVertex("assets/spirv/StencilVert.spv")
            .SetShaderPathFragment("assets/spirv/StencilFrag.spv")
            .SetVertexInput(mGeometryLayout.VertexLayout, 0, VK_VERTEX_INPUT_RATE_VERTEX)
            .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetPolygonMode(VK_POLYGON_MODE_FILL)
            .SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .EnableStencilTest(stencilWriteState, stencilWriteState)
            .SetStencilFormat(mDepthStencilFormat)
            .EnableDepthTest(VK_COMPARE_OP_ALWAYS)
            .SetDepthFormat(mDepthStencilFormat)
            .SetPushConstantSize(sizeof(OutlinePCData))
            .AddDescriptorSetLayout(mDynamicUBO.DescriptorSetLayout())
            .AddDescriptorSetLayout(mMaterialDescriptorSetLayout)
            .Build(mCtx, mPipelineDeletionQueue);

    VkStencilOpState stencilOutlineState{
        .failOp = VK_STENCIL_OP_KEEP,
        .passOp = VK_STENCIL_OP_REPLACE,
        .depthFailOp = VK_STENCIL_OP_KEEP,
        .compareOp = VK_COMPARE_OP_NOT_EQUAL,
        .compareMask = ~0u,
        .writeMask = 0u,
        .reference = 1,
    };

    mOutlinePipeline =
        PipelineBuilder("MinimalPBRStencilPipeline")
            .SetShaderPathVertex("assets/spirv/OutlineVert.spv")
            .SetShaderPathFragment("assets/spirv/OutlineFrag.spv")
            .SetVertexInput(mGeometryLayout.VertexLayout, 0, VK_VERTEX_INPUT_RATE_VERTEX)
            .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetPolygonMode(VK_POLYGON_MODE_FILL)
            .SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .SetColorFormat(mRenderTargetFormat)
            .EnableStencilTest(stencilOutlineState, stencilOutlineState)
            .SetStencilFormat(mDepthStencilFormat)
            .EnableDepthTest(VK_COMPARE_OP_ALWAYS)
            .SetDepthFormat(mDepthStencilFormat)
            .SetPushConstantSize(sizeof(OutlinePCData))
            .AddDescriptorSetLayout(mDynamicUBO.DescriptorSetLayout())
            .AddDescriptorSetLayout(mMaterialDescriptorSetLayout)
            .Build(mCtx, mPipelineDeletionQueue);
}

void MinimalPbrRenderer::OnUpdate([[maybe_unused]] float deltaTime)
{
    auto &camFr = mCamera.GetFrustum();

    // Construct worldspace coords for the shadow sampling part of the frustum:
    auto GetFarVector = [&](glm::vec4 near, glm::vec4 far) {
        auto normalized = glm::normalize(glm::vec3(far - near));
        return near + glm::vec4(mShadowDist * normalized, 0.0f);
    };

    Frustum shadowedFrustum = camFr;
    shadowedFrustum.FarTopLeft = GetFarVector(camFr.NearTopLeft, camFr.FarTopLeft);
    shadowedFrustum.FarTopRight = GetFarVector(camFr.NearTopRight, camFr.FarTopRight);
    shadowedFrustum.FarBottomLeft =
        GetFarVector(camFr.NearBottomLeft, camFr.FarBottomLeft);
    shadowedFrustum.FarBottomRight =
        GetFarVector(camFr.NearBottomRight, camFr.FarBottomRight);

    // Construct light view matrix, looking along the light direction:
    auto lightDir = mEnvHandler.GetUboData().LightDir;

    glm::mat4 lightView = glm::lookAt(lightDir, glm::vec3(0.0f), glm::vec3(0, -1, 0));

    // Transform the shadowed frustum to light view space:
    auto frustumVertices = shadowedFrustum.GetVertices();

    for (auto &vert : frustumVertices)
    {
        vert = lightView * vert;
    }

    // Find the extents of the frustum in light view space to get ortho projection bounds:
    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float minZ = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float maxY = std::numeric_limits<float>::lowest();
    float maxZ = std::numeric_limits<float>::lowest();

    for (auto &vert : frustumVertices)
    {
        minX = std::min(minX, vert.x);
        minY = std::min(minY, vert.y);
        minZ = std::min(minZ, vert.z);
        maxX = std::max(maxX, vert.x);
        maxY = std::max(maxY, vert.y);
        maxZ = std::max(maxZ, vert.z);
    }

    // Adjust with user controlled parameters:
    maxZ += mAddZ;
    minZ -= mSubZ;

    glm::mat4 lightProj = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);

    // Update light/camera uniform buffer data:
    mUBOData.CameraViewProjection = mCamera.GetViewProj();
    mUBOData.LightViewProjection = lightProj * lightView;
    mUBOData.ViewPos = mCamera.GetPos();
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

void MinimalPbrRenderer::OnRender([[maybe_unused]] std::optional<SceneKey> highlightedObj)
{
    auto &cmd = mFrame.CurrentCmd();

    // This is not OnUpdate since, uniform buffers are per-image index
    // and as such need to be acquired after new image index is set.
    mDynamicUBO.UpdateData(&mUBOData, sizeof(mUBOData));

    DrawStats stats{};

    ShadowPass(cmd, stats);
    MainPass(cmd, stats);

    if (highlightedObj.has_value())
        OutlinePass(cmd, *highlightedObj);

    mFrame.Stats.NumTriangles = stats.NumIdx / 3;
    mFrame.Stats.NumDraws = stats.NumDraws;
    mFrame.Stats.NumBinds = stats.NumBinds;
}

void MinimalPbrRenderer::DrawAllInstances(VkCommandBuffer cmd, Drawable &drawable,
                                          bool shadowPass, DrawStats &stats)
{
    // Bail immediately if there are no instances to draw:
    if (drawable.Instances.empty())
        return;

    auto viewProj =
        shadowPass ? mUBOData.LightViewProjection : mUBOData.CameraViewProjection;

    // If there is only one instance and its out of view, bail before
    // issuing per-drawable bind commands:
    if (drawable.Instances.size() == 1)
    {
        if (!drawable.Bbox.InView(viewProj * drawable.Instances[0]))
        {
            return;
        }
    }

    // Bind all per-drawable resources:
    auto layout = shadowPass ? mShadowmapPipeline.Layout : mMainPipeline.Layout;

    VkBuffer vertBuffer = drawable.VertexBuffer.Handle;
    VkDeviceSize vertOffset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertBuffer, &vertOffset);

    vkCmdBindIndexBuffer(cmd, drawable.IndexBuffer.Handle, 0, mGeometryLayout.IndexType);

    auto &material = mMaterials.at(drawable.MaterialKey);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 1, 1,
                            &material.DescriptorSet, 0, nullptr);

    // Push per-instance data and issue draw commands:
    for (auto &model : drawable.Instances)
    {
        // Do frustum culling:
        if (!drawable.Bbox.InView(viewProj * model))
            continue;

        if (shadowPass)
        {
            ShadowPCData pcData{
                .LightMVP = mUBOData.LightViewProjection * model,
            };

            vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_ALL_GRAPHICS, 0,
                               sizeof(pcData), &pcData);
        }
        else
        {
            MainPCData pcData{
                .Model = model,
                .Normal = glm::transpose(glm::inverse(model)),
            };

            vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_ALL_GRAPHICS, 0,
                               sizeof(pcData), &pcData);
        }

        vkCmdDrawIndexed(cmd, drawable.IndexCount, 1, 0, 0, 0);

        stats.NumDraws++;
        stats.NumIdx += drawable.IndexCount;
    }

    stats.NumBinds += 3;
}

void MinimalPbrRenderer::DrawSingleInstanceOutline(VkCommandBuffer cmd,
                                                   DrawableKey drawableKey,
                                                   size_t instanceId,
                                                   VkPipelineLayout layout)
{
    auto &drawable = mDrawables[drawableKey];
    auto &material = mMaterials.at(drawable.MaterialKey);
    auto &model = drawable.Instances.at(instanceId);

    // Do frustum culling:
    if (!drawable.Bbox.InView(mUBOData.CameraViewProjection * model))
        return;

    // Bind all per-drawable resources:
    VkBuffer vertBuffer = drawable.VertexBuffer.Handle;
    VkDeviceSize vertOffset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertBuffer, &vertOffset);

    vkCmdBindIndexBuffer(cmd, drawable.IndexBuffer.Handle, 0, mGeometryLayout.IndexType);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 1, 1,
                            &material.DescriptorSet, 0, nullptr);

    // Push per-instance data:
    OutlinePCData pcData{
        .Model = model,
    };

    vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_ALL_GRAPHICS, 0, sizeof(pcData),
                       &pcData);

    // Draw:
    vkCmdDrawIndexed(cmd, drawable.IndexCount, 1, 0, 0, 0);
}

void MinimalPbrRenderer::ShadowPass(VkCommandBuffer cmd, DrawStats &stats)
{
    auto extent = VkExtent2D(mShadowmap.Info.extent.width, mShadowmap.Info.extent.height);

    barrier::ImageBarrierDepthToRender(cmd, mShadowmap.Handle);

    VkClearValue depthClear;
    depthClear.depthStencil = {1.0f, 0};

    auto depthAttachment = vkinit::CreateAttachmentInfo(
        mShadowmapView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, depthClear);

    VkRenderingInfo renderingInfo =
        vkinit::CreateRenderingInfoDepthOnly(extent, depthAttachment);

    vkCmdBeginRendering(cmd, &renderingInfo);
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          mShadowmapPipeline.Handle);
        common::ViewportScissor(cmd, extent);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                mShadowmapPipeline.Layout, 0, 1,
                                mDynamicUBO.DescriptorSet(), 0, nullptr);

        stats.NumBinds += 1;

        // Draw single-sided drawables:
        vkCmdSetCullMode(cmd, VK_CULL_MODE_BACK_BIT);

        for (auto key : mSingleSidedDrawableKeys)
        {
            DrawAllInstances(cmd, mDrawables[key], true, stats);
        }

        // Draw double-sided drawables:
        vkCmdSetCullMode(cmd, VK_CULL_MODE_NONE);

        for (auto key : mDoubleSidedDrawableKeys)
        {
            DrawAllInstances(cmd, mDrawables[key], true, stats);
        }
    }
    vkCmdEndRendering(cmd);

    barrier::ImageBarrierDepthToSample(cmd, mShadowmap.Handle);
}

void MinimalPbrRenderer::MainPass(VkCommandBuffer cmd, DrawStats &stats)
{
    VkClearValue clear;
    clear.color = {{0.0f, 0.0f, 0.0f, 0.0f}};

    auto colorAttachment = vkinit::CreateAttachmentInfo(
        mRenderTargetView, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, clear);

    VkClearValue depthClear;
    depthClear.depthStencil = {1.0f, 0};

    auto depthAttachment = vkinit::CreateAttachmentInfo(
        mDepthStencilBufferView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        depthClear);

    VkRenderingInfo renderingInfo = vkinit::CreateRenderingInfo(
        GetTargetSize(), colorAttachment, depthAttachment, true);

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
                                mMainPipeline.Layout, 3, 1, &mShadowmapDescriptorSet, 0,
                                nullptr);

        stats.NumBinds += 2;

        // Draw single-sided drawables:
        vkCmdSetCullMode(cmd, VK_CULL_MODE_BACK_BIT);

        for (auto key : mSingleSidedDrawableKeys)
        {
            DrawAllInstances(cmd, mDrawables[key], false, stats);
        }

        // Draw double-sided drawables:
        vkCmdSetCullMode(cmd, VK_CULL_MODE_NONE);

        for (auto key : mDoubleSidedDrawableKeys)
        {
            DrawAllInstances(cmd, mDrawables[key], false, stats);
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
                               VK_SHADER_STAGE_ALL_GRAPHICS, 0, sizeof(FrustumBack),
                               &mCamera.GetFrustumBack());

            vkCmdDraw(cmd, 3, 1, 0, 0);

            stats.NumBinds += 2;
            stats.NumDraws += 1;
        }
    }
    vkCmdEndRendering(cmd);
}

void MinimalPbrRenderer::OutlinePass(VkCommandBuffer cmd, SceneKey highlightedObj)
{
    // Update selected drawables list if necessary:
    if (highlightedObj != mLastHighlightedObjKey)
    {
        mSelectedDrawableKeys.clear();

        for (auto &[key, list] : mObjectCache)
        {
            if (key == highlightedObj)
            {
                for (auto value : list)
                    mSelectedDrawableKeys.push_back(value);
            }
        }

        mLastHighlightedObjKey = highlightedObj;
    }

    // Draw to stencil:
    {
        auto stencilAttachment = vkinit::CreateAttachmentInfo(
            mDepthStencilBufferView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

        VkRenderingInfo renderingInfo =
            vkinit::CreateRenderingInfoDepthStencil(GetTargetSize(), stencilAttachment);

        vkCmdBeginRendering(cmd, &renderingInfo);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mStencilPipeline.Handle);
        common::ViewportScissor(cmd, GetTargetSize());

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                mStencilPipeline.Layout, 0, 1,
                                mDynamicUBO.DescriptorSet(), 0, nullptr);

        for (auto [drawableKey, instanceId] : mSelectedDrawableKeys)
        {
            DrawSingleInstanceOutline(cmd, drawableKey, instanceId,
                                      mStencilPipeline.Layout);
        }

        vkCmdEndRendering(cmd);
    }

    // Draw outline:
    {
        auto colorAttachment = vkinit::CreateAttachmentInfo(
            mRenderTargetView, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        auto stencilAttachment = vkinit::CreateAttachmentInfo(
            mDepthStencilBufferView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

        VkRenderingInfo renderingInfo = vkinit::CreateRenderingInfo(
            GetTargetSize(), colorAttachment, stencilAttachment, true);

        vkCmdBeginRendering(cmd, &renderingInfo);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mOutlinePipeline.Handle);
        common::ViewportScissor(cmd, GetTargetSize());

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                mOutlinePipeline.Layout, 0, 1,
                                mDynamicUBO.DescriptorSet(), 0, nullptr);

        for (auto [drawableKey, instanceId] : mSelectedDrawableKeys)
        {
            DrawSingleInstanceOutline(cmd, drawableKey, instanceId,
                                      mOutlinePipeline.Layout);
        }

        vkCmdEndRendering(cmd);
    }
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
        .Layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
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
        .Format = mDepthStencilFormat,
        .Tiling = VK_IMAGE_TILING_OPTIMAL,
        .Usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .MipLevels = 1,
        .Layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    mDepthStencilBuffer = MakeImage::Image2D(mCtx, "DepthBuffer", depthBufferInfo);
    mDepthStencilBufferView = MakeView::View2D(
        mCtx, "DepthBufferView", mDepthStencilBuffer, mDepthStencilFormat,
        VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);

    mSwapchainDeletionQueue.push_back(mDepthStencilBuffer);
    mSwapchainDeletionQueue.push_back(mDepthStencilBufferView);
}

void MinimalPbrRenderer::LoadScene(const Scene &scene)
{
    if (scene.FullReload())
    {
        for (auto &[_, drawable] : mDrawables)
            DestroyDrawable(drawable);

        for (auto &[_, texture] : mImages)
            DestroyTexture(texture);

        mDrawables.clear();
        mMaterials.clear();
        mImages.clear();

        mMaterialDescriptorAllocator.DestroyPools();
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
                auto &geo = prim.Data;
                auto &drawable = mDrawables[drawableKey];

                const auto debugName = mesh.Name + std::to_string(primIdx);

                // Create Vertex buffer:
                drawable.VertexBuffer =
                    MakeBuffer::Vertex(mCtx, debugName, geo.VertexData);
                drawable.VertexCount = static_cast<uint32_t>(geo.VertexData.Count);

                // Create Index buffer:
                drawable.IndexBuffer = MakeBuffer::Index(mCtx, debugName, geo.IndexData);
                drawable.IndexCount = static_cast<uint32_t>(geo.IndexData.Count);

                drawable.Bbox = prim.Data.BBox;
            }
        }
    }

    // Prune orphaned drawables:
    std::erase_if(mDrawables, [&](const auto &item) {
        auto &drawable = item.second;

        auto meshKey = item.first.first;
        bool erase = scene.Meshes.count(meshKey) == 0;

        if (erase)
            DestroyDrawable(drawable);

        return erase;
    });
}

void MinimalPbrRenderer::LoadImages(const Scene &scene)
{
    for (auto &[key, imgData] : scene.Images)
    {
        const bool alreadyLoaded = mImages.count(key) != 0;

        if (alreadyLoaded && imgData.IsUpToDate)
            continue;

        auto &texture = mImages[key];

        if (alreadyLoaded)
        {
            DestroyTexture(texture);
        }

        texture = TextureLoaders::LoadTexture2DMipped(mCtx, "MaterialTexture", imgData);
        imgData.IsUpToDate = true;
    }

    // Prune orphaned images:
    std::erase_if(mImages, [&](const auto &item) {
        auto &key = item.first;
        auto &img = item.second;

        bool erase = scene.Images.count(key) == 0;

        if (erase)
            DestroyTexture(img);

        return erase;
    });
}

void MinimalPbrRenderer::LoadMaterials(const Scene &scene)
{
    for (auto &[key, sceneMat] : scene.Materials)
    {
        const bool firstLoad = mMaterials.count(key) == 0;
        auto &mat = mMaterials[key];

        // Only allocate new descriptor set on first load:
        if (firstLoad)
        {
            mat.DescriptorSet =
                mMaterialDescriptorAllocator.Allocate(mMaterialDescriptorSetLayout);

            std::string bufName = "Material " + sceneMat.Name + "UBO";
            mat.UBO = MakeBuffer::MappedUniform(mCtx, bufName, sizeof(mat.UboData));

            mSceneDeletionQueue.push_back(mat.UBO);
        }

        // Update the non-image parameters:
        mat.UboData.AlphaCutoff = sceneMat.AlphaCutoff;
        mat.UboData.DoubleSided = sceneMat.DoubleSided;

        if (sceneMat.TranslucentColor.has_value())
            mat.UboData.TranslucentColor = *sceneMat.TranslucentColor;

        Buffer::UploadToMapped(mat.UBO, &mat.UboData, sizeof(mat.UboData));

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
            .WriteUniformBuffer(3, mat.UBO.Handle, sizeof(mat.UboData))
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

                if (mat.UboData.DoubleSided)
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

    // Load all object transforms and build object index cache:
    mObjectCache.clear();

    for (auto &[_, drawable] : mDrawables)
        drawable.Instances.clear();

    for (const auto &[objKey, obj] : scene.Objects)
    {
        if (!obj.Mesh.has_value())
            continue;

        auto meshKey = *obj.Mesh;

        for (const auto [primIdx, _] : enumerate(scene.Meshes.at(meshKey).Primitives))
        {
            auto drawableKey = DrawableKey{meshKey, primIdx};

            if (mDrawables.count(drawableKey) == 0)
            {
                continue;
            }

            auto &drawable = mDrawables[drawableKey];

            auto &list = mObjectCache[objKey];
            list.emplace_back(drawableKey, drawable.Instances.size());

            drawable.Instances.push_back(obj.Transform);
        }
    }
}