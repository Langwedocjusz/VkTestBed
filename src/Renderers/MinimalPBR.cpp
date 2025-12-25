#include "MinimalPBR.h"
#include "Camera.h"
#include "GeometryData.h"
#include "Pch.h"

#include "BufferUtils.h"
#include "Common.h"
#include "Descriptor.h"
#include "ImageLoaders.h"
#include "ImageUtils.h"
#include "Pipeline.h"
#include "Renderer.h"
#include "Sampler.h"
#include "Scene.h"
#include "VulkanContext.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/matrix.hpp>

#include <imgui.h>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <ranges>
#include <utility>
#include <vulkan/vulkan_core.h>

void MinimalPbrRenderer::Drawable::Init(VulkanContext &ctx, const ScenePrimitive &prim,
                                        const std::string &debugName)
{
    auto &geo = prim.Data;

    // Create Vertex buffer:
    VertexBuffer = MakeBuffer::Vertex(ctx, debugName, geo.VertexData);
    VertexCount = static_cast<uint32_t>(geo.VertexData.Count);

    // Create Index buffer:
    IndexBuffer = MakeBuffer::Index(ctx, debugName, geo.IndexData);
    IndexCount = static_cast<uint32_t>(geo.IndexData.Count);

    Bbox = prim.Data.BBox;
}

void MinimalPbrRenderer::Drawable::Destroy(VulkanContext &ctx)
{
    vmaDestroyBuffer(ctx.Allocator, VertexBuffer.Handle, VertexBuffer.Allocation);
    vmaDestroyBuffer(ctx.Allocator, IndexBuffer.Handle, IndexBuffer.Allocation);
}

bool MinimalPbrRenderer::Drawable::EarlyBail(glm::mat4 viewProj)
{
    if (Instances.empty())
        return true;

    if (Instances.size() == 1)
    {
        if (!Bbox.InView(viewProj * Instances[0].Transform))
        {
            return true;
        }
    }

    return false;
}

bool MinimalPbrRenderer::Drawable::IsVisible(glm::mat4 viewProj, size_t instanceIdx)
{
    return Bbox.InView(viewProj * Instances[instanceIdx].Transform);
}

void MinimalPbrRenderer::Drawable::BindGeometryBuffers(VkCommandBuffer cmd)
{
    VkBuffer vertBuffer = VertexBuffer.Handle;
    VkDeviceSize vertOffset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertBuffer, &vertOffset);

    vkCmdBindIndexBuffer(cmd, IndexBuffer.Handle, 0, MinimalPbrRenderer::IndexType);
}

void MinimalPbrRenderer::Drawable::Draw(VkCommandBuffer cmd)
{
    vkCmdDrawIndexed(cmd, IndexCount, 1, 0, 0, 0);
}

void MinimalPbrRenderer::DestroyTexture(const Texture &texture)
{
    vkDestroyImageView(mCtx.Device, texture.View, nullptr);
    vmaDestroyImage(mCtx.Allocator, texture.Img.Handle, texture.Img.Allocation);
}

MinimalPbrRenderer::MinimalPbrRenderer(VulkanContext &ctx, FrameInfo &info,
                                       Camera &camera)
    : IRenderer(ctx, info, camera), mMaterialDescriptorAllocator(ctx),
      mDynamicUBO(ctx, info), mEnvHandler(ctx), mShadowmapHandler(ctx),
      mSceneDeletionQueue(ctx), mMaterialDeletionQueue(ctx)
{
    // Create the texture samplers:
    mSampler2D = SamplerBuilder("MinimalPbrSampler2D")
                     .SetMagFilter(VK_FILTER_LINEAR)
                     .SetMinFilter(VK_FILTER_LINEAR)
                     .SetAddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT)
                     .SetMipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR)
                     .SetMaxLod(12.0f)
                     .Build(mCtx, mMainDeletionQueue);

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

    // Build dynamic uniform buffers & descriptors:
    auto stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    mDynamicUBO.OnInit("MinimalPBRDynamicUBO", stages, sizeof(mUBOData));

    // Build the graphics pipelines:
    RebuildPipelines();
}

MinimalPbrRenderer::~MinimalPbrRenderer()
{
    mMaterialDescriptorAllocator.DestroyPools();

    for (auto &[_, drawable] : mDrawables)
        drawable.Destroy(mCtx);

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
            .SetPushConstantSize(sizeof(mMainPCData))
            .AddDescriptorSetLayout(mDynamicUBO.DescriptorSetLayout())
            .AddDescriptorSetLayout(mEnvHandler.GetLightingDSLayout())
            .AddDescriptorSetLayout(mShadowmapHandler.GetDSLayout())
            .AddDescriptorSetLayout(mMaterialDescriptorSetLayout)
            .EnableDepthTest()
            .SetDepthFormat(mDepthStencilFormat)
            .SetStencilFormat(mDepthStencilFormat)
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
            .SetStencilFormat(mDepthStencilFormat)
            .Build(mCtx, mPipelineDeletionQueue);

    // Rebuild env handler pipelines as well:
    mEnvHandler.RebuildPipelines();
    mShadowmapHandler.RebuildPipelines(mGeometryLayout.VertexLayout,
                                       mMaterialDescriptorSetLayout);

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
            .SetStencilFormat(mDepthStencilFormat)
            .SetPushConstantSize(sizeof(mOutlinePCData))
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
            .SetPushConstantSize(sizeof(mOutlinePCData))
            .AddDescriptorSetLayout(mDynamicUBO.DescriptorSetLayout())
            .AddDescriptorSetLayout(mMaterialDescriptorSetLayout)
            .Build(mCtx, mPipelineDeletionQueue);

    mObjectIdPipeline =
        PipelineBuilder("MinimalPBRObjectIdPipeline")
            .SetShaderPathVertex("assets/spirv/ObjectIdVert.spv")
            .SetShaderPathFragment("assets/spirv/ObjectIdFrag.spv")
            .SetVertexInput(mGeometryLayout.VertexLayout, 0, VK_VERTEX_INPUT_RATE_VERTEX)
            .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetPolygonMode(VK_POLYGON_MODE_FILL)
            .SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .EnableDepthTest()
            .RequestDynamicState(VK_DYNAMIC_STATE_CULL_MODE)
            .SetColorFormat(PickingTargetFormat)
            .SetDepthFormat(PickingDepthFormat)
            .AddDescriptorSetLayout(mDynamicUBO.DescriptorSetLayout())
            .AddDescriptorSetLayout(mMaterialDescriptorSetLayout)
            .SetPushConstantSize(sizeof(mObjectIdPCData))
            .Build(mCtx, mPipelineDeletionQueue);
}

void MinimalPbrRenderer::OnUpdate([[maybe_unused]] float deltaTime)
{
    auto lightDir = mEnvHandler.GetUboData().LightDir;
    mShadowmapHandler.OnUpdate(mCamera.GetFrustum(), lightDir);

    // Update light/camera uniform buffer data:
    mUBOData.CameraViewProjection = mCamera.GetViewProj();
    mUBOData.LightViewProjection = mShadowmapHandler.GetViewProj();
    mUBOData.ViewPos = mCamera.GetPos();
}

void MinimalPbrRenderer::OnImGui()
{
    ImGui::Begin("Renderer settings");

    ImGui::SliderFloat("Directional Factor", &mUBOData.DirectionalFactor, 0.0f, 6.0f);
    ImGui::SliderFloat("Environment Factor", &mUBOData.EnvironmentFactor, 0.0f, 1.0f);

    ImGui::SliderFloat("Shadow Bias Min", &mUBOData.ShadowBiasMin, 0.0f, 0.1f);
    ImGui::SliderFloat("Shadow Bias Max", &mUBOData.ShadowBiasMax, 0.0f, 0.1f);

    mShadowmapHandler.OnImGui();

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

template <typename MaterialFn, typename InstanceFn>
void MinimalPbrRenderer::DrawAllInstancesCulled(VkCommandBuffer cmd, Drawable &drawable,
                                                glm::mat4 viewProj,
                                                MaterialFn materialCallback,
                                                InstanceFn instanceCallback,
                                                DrawStats &stats)
{
    using namespace std::views;

    // Bind drawable geometry buffers:
    drawable.BindGeometryBuffers(cmd);

    // Bind drawable material descriptor set:
    auto &material = mMaterials.at(drawable.MaterialKey);
    materialCallback(cmd, material);

    // Push per-instance data and issue draw commands:
    for (auto [idx, instance] : enumerate(drawable.Instances))
    {
        // Do frustum culling:
        if (!drawable.IsVisible(viewProj, idx))
            continue;

        // Callback for per-instance binds:
        instanceCallback(cmd, instance);

        drawable.Draw(cmd);

        stats.NumDraws++;
        stats.NumIdx += drawable.IndexCount;
    }

    stats.NumBinds += 3;
}

template <typename MaterialFn, typename InstanceFn>
void MinimalPbrRenderer::DrawSceneFrustumCulled(VkCommandBuffer cmd, glm::mat4 viewProj,
                                                MaterialFn materialCallback,
                                                InstanceFn instanceCallback,
                                                DrawStats &stats)
{
    // Draw single-sided drawables:
    vkCmdSetCullMode(cmd, VK_CULL_MODE_BACK_BIT);

    for (auto key : mSingleSidedDrawableKeys)
    {
        auto &drawable = mDrawables[key];

        // Bail immediately if there are no instances to draw:
        if (drawable.EarlyBail(viewProj))
            continue;

        DrawAllInstancesCulled(cmd, drawable, viewProj, materialCallback,
                               instanceCallback, stats);
    }

    // Draw double-sided drawables:
    vkCmdSetCullMode(cmd, VK_CULL_MODE_NONE);

    for (auto key : mDoubleSidedDrawableKeys)
    {
        auto &drawable = mDrawables[key];

        // Bail immediately if there are no instances to draw:
        if (drawable.EarlyBail(viewProj))
            continue;

        DrawAllInstancesCulled(cmd, drawable, viewProj, materialCallback,
                               instanceCallback, stats);
    }
}

void MinimalPbrRenderer::ShadowPass(VkCommandBuffer cmd, DrawStats &stats)
{
    mShadowmapHandler.BeginShadowPass(cmd);
    stats.NumBinds += 2;

    auto viewProj = mUBOData.LightViewProjection;

    auto materialCallback = [this](VkCommandBuffer cmd, Material &material) {
        mShadowmapHandler.BindMaterialDS(cmd, material.DescriptorSet);
    };

    auto instanceCallback = [this](VkCommandBuffer cmd, Instance &instance) {
        mShadowmapHandler.PushConstantTransform(cmd, instance.Transform);
    };

    DrawSceneFrustumCulled(cmd, viewProj, materialCallback, instanceCallback, stats);

    mShadowmapHandler.EndShadowPass(cmd);
}

void MinimalPbrRenderer::MainPass(VkCommandBuffer cmd, DrawStats &stats)
{
    using namespace std::views;

    common::BeginRenderingColorDepth(cmd, GetTargetSize(), mRenderTargetView,
                                            mDepthStencilBufferView, true, true);
    {
        // Draw the scene:
        mMainPipeline.Bind(cmd);
        common::ViewportScissor(cmd, GetTargetSize());

        std::array descriptorSets{
            mDynamicUBO.DescriptorSet(),
            mEnvHandler.GetLightingDS(),
            mShadowmapHandler.GetDescriptorSet(),
        };

        mMainPipeline.BindDescriptorSets(cmd, descriptorSets, 0);

        stats.NumBinds += 3;

        auto viewProj = mUBOData.CameraViewProjection;

        auto materialCallback = [this](VkCommandBuffer cmd, Material &material) {
            mMainPipeline.BindDescriptorSet(cmd, material.DescriptorSet, 3);
        };

        auto instanceCallback = [this](VkCommandBuffer cmd, Instance &instance) {
            mMainPCData.Model = instance.Transform;
            mMainPCData.Normal = glm::transpose(glm::inverse(instance.Transform));

            vkCmdPushConstants(cmd, mMainPipeline.Layout, VK_SHADER_STAGE_ALL_GRAPHICS, 0,
                               sizeof(mMainPCData), &mMainPCData);
        };

        DrawSceneFrustumCulled(cmd, viewProj, materialCallback, instanceCallback, stats);

        // Draw the background:
        if (mEnvHandler.HdriEnabled())
        {
            mBackgroundPipeline.Bind(cmd);
            common::ViewportScissor(cmd, GetTargetSize());

            mBackgroundPipeline.BindDescriptorSet(cmd, mEnvHandler.GetBackgroundDS(), 0);

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
        common::BeginRenderingDepth(cmd, GetTargetSize(), mDepthStencilBufferView, true, false);

        mStencilPipeline.Bind(cmd);
        common::ViewportScissor(cmd, GetTargetSize());

        mStencilPipeline.BindDescriptorSet(cmd, mDynamicUBO.DescriptorSet(), 0);

        for (auto [drawableKey, instanceId] : mSelectedDrawableKeys)
        {
            auto &drawable = mDrawables[drawableKey];

            // Do frustum culling:
            if (!drawable.IsVisible(mUBOData.CameraViewProjection, instanceId))
                break;

            // Bind all per-drawable resources:
            drawable.BindGeometryBuffers(cmd);

            auto &material = mMaterials.at(drawable.MaterialKey);
            mStencilPipeline.BindDescriptorSet(cmd, material.DescriptorSet, 1);

            // Push per-instance data:
            auto &model = drawable.Instances.at(instanceId).Transform;
            mOutlinePCData.Model = model;

            vkCmdPushConstants(cmd, mStencilPipeline.Layout, VK_SHADER_STAGE_ALL_GRAPHICS,
                               0, sizeof(mOutlinePCData), &mOutlinePCData);

            // Draw:
            drawable.Draw(cmd);
        }

        vkCmdEndRendering(cmd);
    }

    // Draw outline:
    {
        common::BeginRenderingColorDepth(cmd, GetTargetSize(), mRenderTargetView, mDepthStencilBufferView, true, false);

        mOutlinePipeline.Bind(cmd);
        common::ViewportScissor(cmd, GetTargetSize());

        mOutlinePipeline.BindDescriptorSet(cmd, mDynamicUBO.DescriptorSet(), 0);

        for (auto [drawableKey, instanceId] : mSelectedDrawableKeys)
        {
            auto &drawable = mDrawables[drawableKey];

            // Do frustum culling:
            if (!drawable.IsVisible(mUBOData.CameraViewProjection, instanceId))
                break;

            // Bind all per-drawable resources:
            drawable.BindGeometryBuffers(cmd);

            auto &material = mMaterials.at(drawable.MaterialKey);
            mOutlinePipeline.BindDescriptorSet(cmd, material.DescriptorSet, 1);

            // Push per-instance data:
            auto &model = drawable.Instances.at(instanceId).Transform;
            mOutlinePCData.Model = model;

            vkCmdPushConstants(cmd, mOutlinePipeline.Layout, VK_SHADER_STAGE_ALL_GRAPHICS,
                               0, sizeof(mOutlinePCData), &mOutlinePCData);

            // Draw:
            drawable.Draw(cmd);
        }

        vkCmdEndRendering(cmd);
    }
}

void MinimalPbrRenderer::RenderObjectId(VkCommandBuffer cmd, float x, float y)
{
    // Calculate camera matrix:
    float pixel_dx = 1.0f / static_cast<float>(mRenderTarget.Info.extent.width);
    float pixel_dy = 1.0f / static_cast<float>(mRenderTarget.Info.extent.height);

    float xmin = x * pixel_dx * mInternalResolutionScale;
    float ymin = y * pixel_dy * mInternalResolutionScale;

    float xmax = xmin + pixel_dx;
    float ymax = ymin + pixel_dy;

    glm::mat4 viewProj = mCamera.GetViewProjRestrictedRange(xmin, xmax, ymin, ymax);

    // Draw all drawables, outputting their object id as fragment color:
    mObjectIdPipeline.Bind(cmd);
    common::ViewportScissor(cmd, VkExtent2D{1, 1});

    mObjectIdPipeline.BindDescriptorSet(cmd, mDynamicUBO.DescriptorSet(), 0);

    auto materialCallback = [this](VkCommandBuffer cmd, Material &material) {
        mObjectIdPipeline.BindDescriptorSet(cmd, material.DescriptorSet, 1);
    };

    auto instanceCallback = [this, viewProj](VkCommandBuffer cmd, Instance &instance) {
        mObjectIdPCData.Model = viewProj * instance.Transform;
        mObjectIdPCData.ObjectId = instance.ObjectId;

        vkCmdPushConstants(cmd, mObjectIdPipeline.Layout, VK_SHADER_STAGE_ALL_GRAPHICS, 0,
                           sizeof(mObjectIdPCData), &mObjectIdPCData);
    };

    DrawStats stats;

    DrawSceneFrustumCulled(cmd, viewProj, materialCallback, instanceCallback, stats);
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
            drawable.Destroy(mCtx);

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
                const auto debugName = mesh.Name + std::to_string(primIdx);

                auto &drawable = mDrawables[drawableKey];
                drawable.Init(mCtx, prim, debugName);
            }
        }
    }

    // Prune orphaned drawables:
    std::erase_if(mDrawables, [&](auto &item) {
        auto &drawable = item.second;

        auto meshKey = item.first.first;
        bool erase = scene.Meshes.count(meshKey) == 0;

        if (erase)
            drawable.Destroy(mCtx);

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

            drawable.Instances.emplace_back(objKey, obj.Transform);
        }
    }
}