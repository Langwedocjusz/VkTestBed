#include "MinimalPBR.h"
#include "Pch.h"

#include "Camera.h"
#include "Common.h"
#include "Descriptor.h"
#include "GeometryData.h"
#include "ImageLoaders.h"
#include "MakeBuffer.h"
#include "MakeImage.h"
#include "Pipeline.h"
#include "Renderer.h"
#include "Sampler.h"
#include "Scene.h"
#include "VulkanContext.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/matrix.hpp>

#include <imgui.h>

#include "volk.h"

#include <cstdint>
#include <optional>
#include <ranges>
#include <utility>

void MinimalPbrRenderer::Drawable::Init(VulkanContext &ctx, const ScenePrimitive &prim,
                                        const std::string &debugName)
{
    auto &geo = prim.Data;

    // Create Vertex buffer:
    // TODO: restore this codepath:
    // VertexBuffer = MakeBuffer::Vertex(ctx, debugName, geo.VertexData);
    // VertexCount  = static_cast<uint32_t>(geo.VertexCount);

    VertexBuffer = MakeBuffer::VertexStorage(ctx, debugName, geo.VertexData);
    VertexCount  = static_cast<uint32_t>(geo.VertexCount);

    // Retrieve vertex buffer address:
    VkBufferDeviceAddressInfo deviceAdressInfo{
        .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext  = nullptr,
        .buffer = VertexBuffer.Handle,
    };

    VertexAddress = vkGetBufferDeviceAddress(ctx.Device, &deviceAdressInfo);

    // Create Index buffer:
    IndexBuffer = MakeBuffer::Index(ctx, debugName, geo.IndexData);
    IndexCount  = static_cast<uint32_t>(geo.IndexCount);

    Bbox            = prim.Data.BBox;
    TexBoundsCenter = prim.TexCoordCenter;
    TexBoundsExtent = prim.TexCoordExtent;
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
        if (!Bbox.IsInView(viewProj * Instances[0].Transform))
        {
            return true;
        }
    }

    return false;
}

bool MinimalPbrRenderer::Drawable::IsVisible(glm::mat4 viewProj, size_t instanceIdx)
{
    return Bbox.IsInView(viewProj * Instances[instanceIdx].Transform);
}

void MinimalPbrRenderer::Drawable::BindGeometryBuffers(VkCommandBuffer cmd)
{
    // TODO: restore this codepath:
    // VkBuffer     vertBuffer = VertexBuffer.Handle;
    // VkDeviceSize vertOffset = 0;
    // vkCmdBindVertexBuffers(cmd, 0, 1, &vertBuffer, &vertOffset);

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
      mCamDynamicUBO(ctx, info, sizeof(mCamDynamicUBO)),
      mDynamicUBO(ctx, info, sizeof(mUBOData)), mDynamicDS(ctx, info), mEnvHandler(ctx),
      mShadowmapHandler(ctx), mAOHandler(ctx), mSceneDeletionQueue(ctx),
      mMaterialDeletionQueue(ctx)
{
    // Create the material texture sampler:
    mMaterialSampler = SamplerBuilder("MinimalPbrMaterialSampler")
                           .SetMagFilter(VK_FILTER_LINEAR)
                           .SetMinFilter(VK_FILTER_LINEAR)
                           .SetAddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT)
                           .SetMipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR)
                           .SetMaxLod(12.0f)
                           .EnableMaxAnisotropy()
                           .Build(mCtx, mMainDeletionQueue);

    // Create descriptor set layout for sampling material textures:
    {
        auto [layout, _] =
            DescriptorSetLayoutBuilder("MinimalPBRMaterialDescriptorLayout")
                .AddCombinedSampler(0, VK_SHADER_STAGE_FRAGMENT_BIT)
                .AddCombinedSampler(1, VK_SHADER_STAGE_FRAGMENT_BIT)
                .AddCombinedSampler(2, VK_SHADER_STAGE_FRAGMENT_BIT)
                .AddUniformBuffer(3, VK_SHADER_STAGE_FRAGMENT_BIT)
                .Build(ctx, mMainDeletionQueue);

        mMaterialDescriptorSetLayout = layout;
    }

    // Initialize descriptor allocator for materials:
    {
        // clang-format off
        std::array<VkDescriptorPoolSize, 2> poolCounts{{
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
            {        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
        }};
        // clang-format on

        mMaterialDescriptorAllocator.OnInit(poolCounts);
    }

    // Create the default textures:
    auto albedoData    = ImageData::SinglePixel(Pixel{255, 255, 255, 255}, false);
    auto roughnessData = ImageData::SinglePixel(Pixel{0, 255, 255, 0}, true);
    auto normalData    = ImageData::SinglePixel(Pixel{128, 128, 255, 0}, true);

    mDefaultAlbedo = TextureLoaders::LoadTexture2D(mCtx, "DefaultAlbedo", albedoData);
    mDefaultRoughness =
        TextureLoaders::LoadTexture2D(mCtx, "DefaultRoughness", roughnessData);
    mDefaultNormal = TextureLoaders::LoadTexture2D(mCtx, "DefaultNormal", normalData);

    mMainDeletionQueue.push_back(mDefaultAlbedo);
    mMainDeletionQueue.push_back(mDefaultRoughness);
    mMainDeletionQueue.push_back(mDefaultNormal);

    // Build the descriptor sets for the dynamic uniform buffer:
    {
        auto stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        auto [layout, counts] =
            DescriptorSetLayoutBuilder("MinimalPBRDynamicDescriptorSetLayout")
                .AddUniformBuffer(0, stageFlags)
                .AddUniformBuffer(1, stageFlags)
                .Build(mCtx, mMainDeletionQueue);

        mDynamicDS.Initialize(layout, counts);

        // Update descriptor sets:
        mDynamicDS.BeginUpdate();

        {
            auto [buffers, sizes] = mCamDynamicUBO.GetBufferHandlesAndSizes();
            mDynamicDS.WriteUniformBuffer(0, buffers, sizes);
        }
        {
            auto [buffers, sizes] = mDynamicUBO.GetBufferHandlesAndSizes();
            mDynamicDS.WriteUniformBuffer(1, buffers, sizes);
        }

        mDynamicDS.EndUpdate();
    }

    // Build the graphics pipelines:
    RebuildPipelines();
}

MinimalPbrRenderer::~MinimalPbrRenderer()
{
    for (auto &[_, drawable] : mDrawables)
        drawable.Destroy(mCtx);

    for (auto &[_, tex] : mTextures)
        DestroyTexture(tex);
}

void MinimalPbrRenderer::RebuildPipelines()
{
    mPipelineDeletionQueue.flush();

    mZPrepassOpaquePipeline =
        PipelineBuilder("MinimalPBROpaquePrepassPipeline")
            .SetShaderPathVertex("assets/spirv/ZPrepassOpaqueVert.spv")
            // TODO: restore this code in separate path:
            //.SetVertexInput(mGeometryLayout.VertexLayout, 0,
            // VK_VERTEX_INPUT_RATE_VERTEX)
            .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetPolygonMode(VK_POLYGON_MODE_FILL)
            .SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .RequestDynamicState(VK_DYNAMIC_STATE_CULL_MODE)
            .SetPushConstantSize(sizeof(PCDataPrepass))
            .AddDescriptorSetLayout(mDynamicDS.DescriptorSetLayout())
            .EnableDepthTest()
            .SetDepthFormat(DepthStencilFormat)
            .SetStencilFormat(DepthStencilFormat)
            .SetMultisampling(mMultisample)
            .Build(mCtx, mPipelineDeletionQueue);

    mZPrepassAlphaPipeline =
        PipelineBuilder("MinimalPBRAlphaPrepassPipeline")
            .SetShaderPathVertex("assets/spirv/ZPrepassAlphaVert.spv")
            .SetShaderPathFragment("assets/spirv/ZPrepassAlphaFrag.spv")
            // TODO: restore this code in separate path:
            //.SetVertexInput(mGeometryLayout.VertexLayout, 0,
            // VK_VERTEX_INPUT_RATE_VERTEX)
            .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetPolygonMode(VK_POLYGON_MODE_FILL)
            .SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .RequestDynamicState(VK_DYNAMIC_STATE_CULL_MODE)
            .SetPushConstantSize(sizeof(PCDataPrepass))
            .AddDescriptorSetLayout(mDynamicDS.DescriptorSetLayout())
            .AddDescriptorSetLayout(mMaterialDescriptorSetLayout)
            .EnableDepthTest()
            .SetDepthFormat(DepthStencilFormat)
            .SetStencilFormat(DepthStencilFormat)
            .SetMultisampling(mMultisample)
            .Build(mCtx, mPipelineDeletionQueue);

    VkCompareOp mainCompareOp{};

    if (mEnablePrepass)
        mainCompareOp = VK_COMPARE_OP_EQUAL;
    else
        mainCompareOp = VK_COMPARE_OP_LESS;

    mMainPipeline =
        PipelineBuilder("MinimalPBRMainPipeline")
            .SetShaderPathVertex("assets/spirv/MinimalPBRVert.spv")
            .SetShaderPathFragment("assets/spirv/MinimalPBRFrag.spv")
            // TODO: restore this code in separate path:
            //.SetVertexInput(mGeometryLayout.VertexLayout, 0,
            // VK_VERTEX_INPUT_RATE_VERTEX)
            .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetPolygonMode(VK_POLYGON_MODE_FILL)
            .SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .RequestDynamicState(VK_DYNAMIC_STATE_CULL_MODE)
            .SetColorFormat(RenderTargetFormat)
            .SetPushConstantSize(sizeof(PCDataMain))
            .AddDescriptorSetLayout(mDynamicDS.DescriptorSetLayout())
            .AddDescriptorSetLayout(mEnvHandler.GetLightingDSLayout())
            .AddDescriptorSetLayout(mShadowmapHandler.GetDSLayout())
            .AddDescriptorSetLayout(mAOHandler.GetDSLayout())
            .AddDescriptorSetLayout(mMaterialDescriptorSetLayout)
            .EnableDepthTest(mainCompareOp)
            .SetDepthFormat(DepthStencilFormat)
            .SetStencilFormat(DepthStencilFormat)
            .SetMultisampling(mMultisample)
            .Build(mCtx, mPipelineDeletionQueue);

    VkStencilOpState stencilWriteState{
        .failOp      = VK_STENCIL_OP_REPLACE,
        .passOp      = VK_STENCIL_OP_REPLACE,
        .depthFailOp = VK_STENCIL_OP_REPLACE,
        .compareOp   = VK_COMPARE_OP_ALWAYS,
        .compareMask = ~0u,
        .writeMask   = ~0u,
        .reference   = 1,
    };

    mStencilPipeline =
        PipelineBuilder("MinimalPBRStencilPipeline")
            .SetShaderPathVertex("assets/spirv/outline/StencilVert.spv")
            .SetShaderPathFragment("assets/spirv/outline/StencilFrag.spv")
            // TODO: restore this code in separate path:
            //.SetVertexInput(mGeometryLayout.VertexLayout, 0,
            // VK_VERTEX_INPUT_RATE_VERTEX)
            .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetPolygonMode(VK_POLYGON_MODE_FILL)
            .SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .EnableStencilTest(stencilWriteState, stencilWriteState)
            .SetStencilFormat(DepthStencilFormat)
            .EnableDepthTest(VK_COMPARE_OP_ALWAYS)
            .SetDepthFormat(DepthStencilFormat)
            .SetStencilFormat(DepthStencilFormat)
            .SetPushConstantSize(sizeof(PCDataOutline))
            .AddDescriptorSetLayout(mDynamicDS.DescriptorSetLayout())
            .AddDescriptorSetLayout(mMaterialDescriptorSetLayout)
            .SetMultisampling(mMultisample)
            .Build(mCtx, mPipelineDeletionQueue);

    VkStencilOpState stencilOutlineState{
        .failOp      = VK_STENCIL_OP_KEEP,
        .passOp      = VK_STENCIL_OP_REPLACE,
        .depthFailOp = VK_STENCIL_OP_KEEP,
        .compareOp   = VK_COMPARE_OP_NOT_EQUAL,
        .compareMask = ~0u,
        .writeMask   = 0u,
        .reference   = 1,
    };

    mOutlinePipeline =
        PipelineBuilder("MinimalPBRStencilPipeline")
            .SetShaderPathVertex("assets/spirv/outline/OutlineVert.spv")
            .SetShaderPathFragment("assets/spirv/outline/OutlineFrag.spv")
            // TODO: restore this code in separate path:
            //.SetVertexInput(mGeometryLayout.VertexLayout, 0,
            // VK_VERTEX_INPUT_RATE_VERTEX)
            .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetPolygonMode(VK_POLYGON_MODE_FILL)
            .SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .SetColorFormat(RenderTargetFormat)
            .EnableStencilTest(stencilOutlineState, stencilOutlineState)
            .SetStencilFormat(DepthStencilFormat)
            .EnableDepthTest(VK_COMPARE_OP_ALWAYS)
            .SetDepthFormat(DepthStencilFormat)
            .SetPushConstantSize(sizeof(PCDataOutline))
            .AddDescriptorSetLayout(mDynamicDS.DescriptorSetLayout())
            .AddDescriptorSetLayout(mMaterialDescriptorSetLayout)
            .SetMultisampling(mMultisample)
            .Build(mCtx, mPipelineDeletionQueue);

    mObjectIdPipeline =
        PipelineBuilder("MinimalPBRObjectIdPipeline")
            .SetShaderPathVertex("assets/spirv/outline/ObjectIdVert.spv")
            .SetShaderPathFragment("assets/spirv/outline/ObjectIdFrag.spv")
            // TODO: restore this code in separate path:
            //.SetVertexInput(mGeometryLayout.VertexLayout, 0,
            // VK_VERTEX_INPUT_RATE_VERTEX)
            .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetPolygonMode(VK_POLYGON_MODE_FILL)
            .SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .EnableDepthTest()
            .RequestDynamicState(VK_DYNAMIC_STATE_CULL_MODE)
            .SetColorFormat(PickingTargetFormat)
            .SetDepthFormat(PickingDepthFormat)
            .AddDescriptorSetLayout(mDynamicDS.DescriptorSetLayout())
            .AddDescriptorSetLayout(mMaterialDescriptorSetLayout)
            .SetPushConstantSize(sizeof(PCDataObjectID))
            .Build(mCtx, mPipelineDeletionQueue);

    // Rebuild component pipelines as well:
    ShadowmapHandler::PipelineInfo info{
        .VertexLayout         = mGeometryLayout.VertexLayout,
        .MaterialDSLayout     = mMaterialDescriptorSetLayout,
        .ColorFormat          = RenderTargetFormat,
        .DepthFormat          = DepthStencilFormat,
        .CurrentMultisampling = mMultisample,
    };

    mEnvHandler.RebuildPipelines(RenderTargetFormat, DepthStencilFormat, mMultisample);
    mShadowmapHandler.RebuildPipelines(info);
    mAOHandler.RebuildPipelines();
}

void MinimalPbrRenderer::RecreateSwapchainResources()
{
    mSwapchainDeletionQueue.flush();
    // TODO: If multisampling is set back to 1, then multisample target optionals can be
    // set back to nullopt

    // Create the render target:
    auto ScaleResolution = [this](uint32_t res) {
        return static_cast<uint32_t>(mInternalResolutionScale * static_cast<float>(res));
    };

    uint32_t width  = ScaleResolution(mCtx.Swapchain.extent.width);
    uint32_t height = ScaleResolution(mCtx.Swapchain.extent.height);

    VkExtent2D drawExtent{
        .width  = width,
        .height = height,
    };

    VkImageUsageFlags drawUsage{};
    drawUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    Image2DInfo renderTargetInfo{
        .Extent = drawExtent,
        .Format = RenderTargetFormat,
        .Usage  = drawUsage,
        .Layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    };
    mRenderTarget = MakeTexture::Texture2D(mCtx, "RenderTarget", renderTargetInfo,
                                           mSwapchainDeletionQueue);

    // Create depth (and stencil) buffer:
    VkImageUsageFlags depthUsage{};
    depthUsage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depthUsage |= VK_IMAGE_USAGE_SAMPLED_BIT;

    Image2DInfo depthBufferInfo{
        .Extent = drawExtent,
        .Format = DepthStencilFormat,
        .Usage  = depthUsage,
        .Layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };
    mDepthStencilBuffer = MakeTexture::Texture2D(mCtx, "DepthBuffer", depthBufferInfo,
                                                 mSwapchainDeletionQueue);

    // Create depth-only view for the depth buffer:
    mDepthOnlyView = MakeView::View2D(mCtx, "DepthOnlyView", mDepthStencilBuffer.Img,
                                      depthBufferInfo.Format, VK_IMAGE_ASPECT_DEPTH_BIT);
    mSwapchainDeletionQueue.push_back(mDepthOnlyView);

    // If multisampling is used, create intermediate buffers for rendering, before
    // resolving into usual images:
    if (mMultisample != VK_SAMPLE_COUNT_1_BIT)
    {
        renderTargetInfo.Multisampling = mMultisample;
        renderTargetInfo.Layout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        mRenderTargetMsaa = MakeTexture::Texture2D(
            mCtx, "RenderTargetMSAA", renderTargetInfo, mSwapchainDeletionQueue);

        depthBufferInfo.Multisampling = mMultisample;
        depthBufferInfo.Layout        = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,

        mDepthStencilMsaa = MakeTexture::Texture2D(
            mCtx, "DepthBufferMSAA", depthBufferInfo, mSwapchainDeletionQueue);
    }

    // Create AO related resources:
    mAOHandler.RecreateSwapchainResources(mDepthStencilBuffer.Img, mDepthOnlyView,
                                          drawExtent);
}

void MinimalPbrRenderer::OnUpdate([[maybe_unused]] float deltaTime)
{
    mShadowmapHandler.OnUpdate(mCamera, mEnvHandler.GetLightDir(), mSceneAABB);

    glm::vec2 drawExtent{
        mRenderTarget.Img.Info.extent.width,
        mRenderTarget.Img.Info.extent.height,
    };

    // Update light/camera uniform buffer data:
    mCamUBOData.CameraViewProjection = mCamera.GetViewProj();
    mCamUBOData.ViewPos              = mCamera.GetPos();
    mCamUBOData.ViewFront            = mCamera.GetFront();

    mUBOData.ShadowMatrices   = mShadowmapHandler.GetMatrices();
    mUBOData.ShadowBounds     = mShadowmapHandler.GetBounds();
    mUBOData.ShadowTexelSizes = mShadowmapHandler.GetTexelSizes();
    mUBOData.AOEnabled        = mEnableAO;
    mUBOData.DrawExtent       = drawExtent;
}

void MinimalPbrRenderer::OnImGui()
{
    ImGui::Begin("Renderer settings");

    if (ImGui::Checkbox("Enable Z Prepass", &mEnablePrepass))
    {
        if (!mEnablePrepass)
            mEnableAO = false;

        vkDeviceWaitIdle(mCtx.Device);

        RecreateSwapchainResources();
        RebuildPipelines();
    }

    if (mEnablePrepass)
    {
        if (ImGui::Checkbox("Enable Ambient Occlusion", &mEnableAO))
        {
            vkDeviceWaitIdle(mCtx.Device);

            RecreateSwapchainResources();
            RebuildPipelines();
        }
    }

    ImGui::SliderFloat("Directional Factor", &mUBOData.DirectionalFactor, 0.0f, 6.0f);
    ImGui::SliderFloat("Environment Factor", &mUBOData.EnvironmentFactor, 0.0f, 1.0f);

    ImGui::SliderFloat("Shadow Bias Light", &mUBOData.ShadowBiasLight, 0.0f, 3.0f);
    ImGui::SliderFloat("Shadow Bias Normal", &mUBOData.ShadowBiasNormal, 0.0f, 3.0f);

    if (ImGui::CollapsingHeader("Render Target"))
    {
        ImGui::SliderFloat("Internal Res Scale", &mInternalResolutionScale, 0.25f, 2.0f);

        static int        choice;
        static std::array names{"1x", "2x", "4x", "8x"};

        ImGui::Combo("Multisampling", &choice, names.data(),
                     static_cast<int32_t>(names.size()));

        if (ImGui::Button("Recreate"))
        {
            vkDeviceWaitIdle(mCtx.Device);

            const std::array options{
                VK_SAMPLE_COUNT_1_BIT,
                VK_SAMPLE_COUNT_2_BIT,
                VK_SAMPLE_COUNT_4_BIT,
                VK_SAMPLE_COUNT_8_BIT,
            };

            mMultisample = options[choice];

            RecreateSwapchainResources();
            // Pipelines also need to be rebuilt when render target changes:
            RebuildPipelines();
        }
    }

    if (ImGui::CollapsingHeader("Shadowmap"))
        mShadowmapHandler.OnImGui();

    if (ImGui::CollapsingHeader("Ambient Occlusion"))
        mAOHandler.OnImGui();

    ImGui::End();
}

void MinimalPbrRenderer::OnRender([[maybe_unused]] std::optional<SceneKey> highlightedObj)
{
    auto &cmd = mFrame.CurrentCmd();

    // This is not OnUpdate since, uniform buffers are per-image index
    // and as such need to be acquired after new image index is set.
    mCamDynamicUBO.UpdateData(&mCamUBOData, sizeof(mCamUBOData));
    mDynamicUBO.UpdateData(&mUBOData, sizeof(mUBOData));

    DrawStats stats{};

    ShadowPass(cmd, stats);

    if (mEnablePrepass)
    {
        Prepass(cmd, stats);

        if (mEnableAO)
            mAOHandler.RunAOPass(cmd, mDepthStencilBuffer.Img, mCamera.GetProj());
    }

    MainPass(cmd, stats);

    if (highlightedObj.has_value())
        OutlinePass(cmd, *highlightedObj);

    mFrame.Stats.NumTriangles = stats.NumIdx / 3;
    mFrame.Stats.NumDraws     = stats.NumDraws;
    mFrame.Stats.NumBinds     = stats.NumBinds;
}

template <typename MaterialFn, typename InstanceFn>
void MinimalPbrRenderer::DrawAllInstancesCulled(VkCommandBuffer cmd, Drawable &drawable,
                                                glm::mat4  viewProj,
                                                MaterialFn materialCallback,
                                                InstanceFn instanceCallback,
                                                DrawStats &stats)
{
    using namespace std::views;

    // If there are no instances to draw, bail before binding anything.
    if (drawable.EarlyBail(viewProj))
        return;

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
        instanceCallback(cmd, drawable, instance);

        drawable.Draw(cmd);

        stats.NumDraws++;
        stats.NumIdx += drawable.IndexCount;
    }

    stats.NumBinds += 3;
}

template <typename MaterialFn, typename InstanceFn>
void MinimalPbrRenderer::DrawSingleSidedFrustumCulled(VkCommandBuffer cmd,
                                                      glm::mat4       viewProj,
                                                      MaterialFn      materialCallback,
                                                      InstanceFn      instanceCallback,
                                                      DrawStats      &stats)
{
    vkCmdSetCullMode(cmd, VK_CULL_MODE_BACK_BIT);

    for (auto key : mSingleSidedDrawableKeys)
    {
        auto &drawable = mDrawables[key];

        DrawAllInstancesCulled(cmd, drawable, viewProj, materialCallback,
                               instanceCallback, stats);
    }
}

template <typename MaterialFn, typename InstanceFn>
void MinimalPbrRenderer::DrawDoubleSidedFrustumCulled(VkCommandBuffer cmd,
                                                      glm::mat4       viewProj,
                                                      MaterialFn      materialCallback,
                                                      InstanceFn      instanceCallback,
                                                      DrawStats      &stats)
{
    vkCmdSetCullMode(cmd, VK_CULL_MODE_NONE);

    for (auto key : mDoubleSidedDrawableKeys)
    {
        auto &drawable = mDrawables[key];

        DrawAllInstancesCulled(cmd, drawable, viewProj, materialCallback,
                               instanceCallback, stats);
    }
}

template <typename MaterialFn, typename InstanceFn>
void MinimalPbrRenderer::DrawSceneFrustumCulled(VkCommandBuffer cmd, glm::mat4 viewProj,
                                                MaterialFn materialCallback,
                                                InstanceFn instanceCallback,
                                                DrawStats &stats)
{
    DrawSingleSidedFrustumCulled(cmd, viewProj, materialCallback, instanceCallback,
                                 stats);
    DrawDoubleSidedFrustumCulled(cmd, viewProj, materialCallback, instanceCallback,
                                 stats);
}

void MinimalPbrRenderer::ShadowPass(VkCommandBuffer cmd, DrawStats &stats)
{
    auto drawOpaque = [&](VkCommandBuffer cmd, glm::mat4 viewProj) {
        auto materialCallback = [](VkCommandBuffer cmd, Material &material) {
            (void)cmd;
            (void)material;
        };

        auto instanceCallback = [&](VkCommandBuffer cmd, Drawable &drawable,
                                    Instance &instance) {
            ShadowmapHandler::PCData data{
                .LightMVP       = viewProj * instance.Transform,
                .VertexBuffer   = drawable.VertexAddress,
                .TexBoundCenter = drawable.TexBoundsCenter,
                .TexBoundExtent = drawable.TexBoundsExtent,
            };

            mShadowmapHandler.PushConstantOpaque(cmd, data);
        };

        DrawSingleSidedFrustumCulled(cmd, viewProj, materialCallback, instanceCallback,
                                     stats);
    };

    auto drawAlpha = [&](VkCommandBuffer cmd, glm::mat4 viewProj) {
        auto materialCallback = [&](VkCommandBuffer cmd, Material &material) {
            mShadowmapHandler.BindAlphaMaterialDS(cmd, material.DescriptorSet);
            stats.NumBinds += 1;
        };

        auto instanceCallback = [&](VkCommandBuffer cmd, Drawable &drawable,
                                    Instance &instance) {
            ShadowmapHandler::PCData data{
                .LightMVP       = viewProj * instance.Transform,
                .VertexBuffer   = drawable.VertexAddress,
                .TexBoundCenter = drawable.TexBoundsCenter,
                .TexBoundExtent = drawable.TexBoundsExtent,
            };

            mShadowmapHandler.PushConstantAlpha(cmd, data);
        };

        DrawDoubleSidedFrustumCulled(cmd, viewProj, materialCallback, instanceCallback,
                                     stats);
    };

    mShadowmapHandler.DrawShadowmaps(cmd, drawOpaque, drawAlpha);
}

void MinimalPbrRenderer::Prepass(VkCommandBuffer cmd, DrawStats &stats)
{
    auto renderInfo = common::RenderingInfo{
        .Extent          = GetTargetSize(),
        .Depth           = mDepthStencilBuffer.View,
        .DepthHasStencil = true,
    };

    if (mMultisample != VK_SAMPLE_COUNT_1_BIT)
    {
        renderInfo.Depth        = mDepthStencilMsaa->View;
        renderInfo.DepthResolve = mDepthStencilBuffer.View;
    }

    common::BeginRendering(cmd, renderInfo);

    auto viewProj = mCamUBOData.CameraViewProjection;

    {
        mZPrepassOpaquePipeline.Bind(cmd);
        common::ViewportScissor(cmd, GetTargetSize());

        mZPrepassOpaquePipeline.BindDescriptorSet(cmd, mDynamicDS.DescriptorSet(), 0);

        auto materialCallback = [](VkCommandBuffer cmd, Material &material) {
            (void)cmd;
            (void)material;
        };

        auto instanceCallback = [this](VkCommandBuffer cmd, Drawable &drawable,
                                       Instance &instance) {
            PCDataPrepass data{
                .Model          = instance.Transform,
                .VertexBuffer   = drawable.VertexAddress,
                .TexBoundCenter = drawable.TexBoundsCenter,
                .TexBoundExtent = drawable.TexBoundsExtent,
            };

            mZPrepassOpaquePipeline.PushConstants(cmd, data);
        };

        DrawSingleSidedFrustumCulled(cmd, viewProj, materialCallback, instanceCallback,
                                     stats);
    }

    {
        mZPrepassAlphaPipeline.Bind(cmd);
        common::ViewportScissor(cmd, GetTargetSize());

        mZPrepassAlphaPipeline.BindDescriptorSet(cmd, mDynamicDS.DescriptorSet(), 0);

        auto materialCallback = [this, &stats](VkCommandBuffer cmd, Material &material) {
            mZPrepassAlphaPipeline.BindDescriptorSet(cmd, material.DescriptorSet, 1);
            stats.NumBinds += 1;
        };

        auto instanceCallback = [this](VkCommandBuffer cmd, Drawable &drawable,
                                       Instance &instance) {
            PCDataPrepass data{
                .Model          = instance.Transform,
                .VertexBuffer   = drawable.VertexAddress,
                .TexBoundCenter = drawable.TexBoundsCenter,
                .TexBoundExtent = drawable.TexBoundsExtent,
            };

            mZPrepassAlphaPipeline.PushConstants(cmd, data);
        };

        DrawDoubleSidedFrustumCulled(cmd, viewProj, materialCallback, instanceCallback,
                                     stats);
    }

    vkCmdEndRendering(cmd);
}

void MinimalPbrRenderer::MainPass(VkCommandBuffer cmd, DrawStats &stats)
{
    auto renderInfo = common::RenderingInfo{
        .Extent          = GetTargetSize(),
        .Color           = mRenderTarget.View,
        .Depth           = mDepthStencilBuffer.View,
        .DepthHasStencil = true,
    };

    if (mEnablePrepass)
        renderInfo.ClearDepth = std::nullopt;

    if (mMultisample != VK_SAMPLE_COUNT_1_BIT)
    {
        renderInfo.Color        = mRenderTargetMsaa->View;
        renderInfo.Depth        = mDepthStencilMsaa->View;
        renderInfo.ColorResolve = mRenderTarget.View;
        renderInfo.DepthResolve = mDepthStencilBuffer.View;
    }

    common::BeginRendering(cmd, renderInfo);

    // Draw the scene:
    mMainPipeline.Bind(cmd);
    common::ViewportScissor(cmd, GetTargetSize());

    std::array descriptorSets{
        mDynamicDS.DescriptorSet(),
        mEnvHandler.GetLightingDS(),
        mShadowmapHandler.GetDescriptorSet(),
        mAOHandler.GetDescriptorSet(),
    };

    mMainPipeline.BindDescriptorSets(cmd, descriptorSets, 0);
    stats.NumBinds += 3;

    auto viewProj = mCamUBOData.CameraViewProjection;

    auto materialCallback = [this, &stats](VkCommandBuffer cmd, Material &material) {
        mMainPipeline.BindDescriptorSet(cmd, material.DescriptorSet, 4);
        stats.NumBinds += 1;
    };

    auto instanceCallback = [this](VkCommandBuffer cmd, Drawable &drawable,
                                   Instance &instance) {
        PCDataMain data{
            .Model          = instance.Transform,
            .VertexBuffer   = drawable.VertexAddress,
            .TexBoundCenter = drawable.TexBoundsCenter,
            .TexBoundExtent = drawable.TexBoundsExtent,
        };

        mMainPipeline.PushConstants(cmd, data);
    };

    DrawSceneFrustumCulled(cmd, viewProj, materialCallback, instanceCallback, stats);

    // At this point debug visuals from the shadow map can optionally be drawn:
    // TODO: This is kind of ugly, but debug needs to have acces to the main depth
    // buffer. Think about a cleaner way to inject debug visualization rendering.
    mShadowmapHandler.DrawDebugShapes(cmd, mCamUBOData.CameraViewProjection,
                                      GetTargetSize());

    // Draw the background:
    auto frustumBack = mCamera.GetFrustumBack();
    mEnvHandler.DrawBackground(cmd, frustumBack, GetTargetSize());

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
    // TODO: this does no check to see if the current drawable is double sided
    // and wheter or not the vkCullState is set accordingly.
    {
        auto renderInfo = common::RenderingInfo{
            .Extent          = GetTargetSize(),
            .Depth           = mDepthStencilBuffer.View,
            .DepthHasStencil = true,
            .ClearDepth      = std::nullopt,
        };

        if (mMultisample != VK_SAMPLE_COUNT_1_BIT)
        {
            renderInfo.Depth        = mDepthStencilMsaa->View;
            renderInfo.DepthResolve = mDepthStencilBuffer.View;
        }

        common::BeginRendering(cmd, renderInfo);

        mStencilPipeline.Bind(cmd);
        common::ViewportScissor(cmd, GetTargetSize());

        mStencilPipeline.BindDescriptorSet(cmd, mDynamicDS.DescriptorSet(), 0);

        for (auto [drawableKey, instanceId] : mSelectedDrawableKeys)
        {
            auto &drawable = mDrawables[drawableKey];

            // Do frustum culling:
            if (!drawable.IsVisible(mCamUBOData.CameraViewProjection, instanceId))
                break;

            // Bind all per-drawable resources:
            drawable.BindGeometryBuffers(cmd);

            auto &material = mMaterials.at(drawable.MaterialKey);
            mStencilPipeline.BindDescriptorSet(cmd, material.DescriptorSet, 1);

            // Push per-instance data:
            auto &model = drawable.Instances.at(instanceId).Transform;

            PCDataOutline data{
                .Model          = model,
                .VertexBuffer   = drawable.VertexAddress,
                .TexBoundCenter = drawable.TexBoundsCenter,
                .TexBoundExtent = drawable.TexBoundsExtent,
            };

            mStencilPipeline.PushConstants(cmd, data);

            // Draw:
            drawable.Draw(cmd);
        }

        vkCmdEndRendering(cmd);
    }

    // Draw outline:
    {
        auto renderInfo = common::RenderingInfo{
            .Extent          = GetTargetSize(),
            .Color           = mRenderTarget.View,
            .Depth           = mDepthStencilBuffer.View,
            .DepthHasStencil = true,
            .ClearColor      = std::nullopt,
            .ClearDepth      = std::nullopt,
        };

        if (mMultisample != VK_SAMPLE_COUNT_1_BIT)
        {
            renderInfo.Color        = mRenderTargetMsaa->View;
            renderInfo.Depth        = mDepthStencilMsaa->View;
            renderInfo.ColorResolve = mRenderTarget.View;
            renderInfo.DepthResolve = mDepthStencilBuffer.View;
        }

        common::BeginRendering(cmd, renderInfo);

        mOutlinePipeline.Bind(cmd);
        common::ViewportScissor(cmd, GetTargetSize());

        mOutlinePipeline.BindDescriptorSet(cmd, mDynamicDS.DescriptorSet(), 0);

        for (auto [drawableKey, instanceId] : mSelectedDrawableKeys)
        {
            auto &drawable = mDrawables[drawableKey];

            // Do frustum culling:
            if (!drawable.IsVisible(mCamUBOData.CameraViewProjection, instanceId))
                break;

            // Bind all per-drawable resources:
            drawable.BindGeometryBuffers(cmd);

            auto &material = mMaterials.at(drawable.MaterialKey);
            mOutlinePipeline.BindDescriptorSet(cmd, material.DescriptorSet, 1);

            // Push per-instance data:
            auto &model = drawable.Instances.at(instanceId).Transform;

            PCDataOutline data{
                .Model          = model,
                .VertexBuffer   = drawable.VertexAddress,
                .TexBoundCenter = drawable.TexBoundsCenter,
                .TexBoundExtent = drawable.TexBoundsExtent,
            };

            mOutlinePipeline.PushConstants(cmd, data);

            // Draw:
            drawable.Draw(cmd);
        }

        vkCmdEndRendering(cmd);
    }
}

void MinimalPbrRenderer::RenderObjectId(VkCommandBuffer cmd, float x, float y)
{
    // Calculate camera matrix:
    float pixel_dx = 1.0f / static_cast<float>(mRenderTarget.Img.Info.extent.width);
    float pixel_dy = 1.0f / static_cast<float>(mRenderTarget.Img.Info.extent.height);

    float xmin = x * pixel_dx * mInternalResolutionScale;
    float ymin = y * pixel_dy * mInternalResolutionScale;

    float xmax = xmin + pixel_dx;
    float ymax = ymin + pixel_dy;

    glm::mat4 viewProj = mCamera.GetViewProjRestrictedRange(xmin, xmax, ymin, ymax);

    // Draw all drawables, outputting their object id as fragment color:
    mObjectIdPipeline.Bind(cmd);
    common::ViewportScissor(cmd, VkExtent2D{1, 1});

    mObjectIdPipeline.BindDescriptorSet(cmd, mDynamicDS.DescriptorSet(), 0);

    auto materialCallback = [this](VkCommandBuffer cmd, Material &material) {
        mObjectIdPipeline.BindDescriptorSet(cmd, material.DescriptorSet, 1);
    };

    auto instanceCallback = [this, viewProj](VkCommandBuffer cmd, Drawable &drawable,
                                             Instance &instance) {
        PCDataObjectID data{
            .Model          = viewProj * instance.Transform,
            .VertexBuffer   = drawable.VertexAddress,
            .TexBoundCenter = drawable.TexBoundsCenter,
            .TexBoundExtent = drawable.TexBoundsExtent,
            .ObjectId       = instance.ObjectId,
        };

        mObjectIdPipeline.PushConstants(cmd, data);
    };

    DrawStats stats;

    DrawSceneFrustumCulled(cmd, viewProj, materialCallback, instanceCallback, stats);
}

void MinimalPbrRenderer::LoadScene(const Scene &scene)
{
    if (scene.FullReloadRequested())
    {
        for (auto &[_, drawable] : mDrawables)
            drawable.Destroy(mCtx);

        for (auto &[_, texture] : mTextures)
            DestroyTexture(texture);

        mDrawables.clear();
        mMaterials.clear();
        mTextures.clear();

        mMaterialDescriptorAllocator.DestroyPools();
    }

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

    if (scene.UpdateEnvironmentRequested())
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

            if (mGeometryLayout == prim.Data.Layout)
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
        bool erase   = scene.Meshes.count(meshKey) == 0;

        if (erase)
            drawable.Destroy(mCtx);

        return erase;
    });
}

void MinimalPbrRenderer::LoadImages(const Scene &scene)
{
    for (auto &[key, imgData] : scene.Images)
    {
        const bool alreadyLoaded = mTextures.count(key) != 0;

        if (alreadyLoaded && imgData.IsUpToDate)
            continue;

        auto &texture = mTextures[key];

        if (alreadyLoaded)
        {
            DestroyTexture(texture);
        }

        texture = TextureLoaders::LoadTexture2D(mCtx, "MaterialTexture", imgData);
        imgData.IsUpToDate = true;
    }

    // Prune orphaned textures:
    std::erase_if(mTextures, [&](const auto &item) {
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
        auto      &mat       = mMaterials[key];

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
                if (mTextures.count(*opt) != 0)
                    return mTextures[*opt];
            }

            return def;
        };

        auto &albedo    = GetTexture(sceneMat.Albedo, mDefaultAlbedo);
        auto &roughness = GetTexture(sceneMat.Roughness, mDefaultRoughness);
        auto &normal    = GetTexture(sceneMat.Normal, mDefaultNormal);

        // Update the descriptor set:
        DescriptorUpdater(mat.DescriptorSet)
            .WriteCombinedSampler(0, albedo.View, mMaterialSampler)
            .WriteCombinedSampler(1, roughness.View, mMaterialSampler)
            .WriteCombinedSampler(2, normal.View, mMaterialSampler)
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
                auto  matKey = *prim.Material;
                auto &mat    = mMaterials[matKey];

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

    // Update scene bounding box:
    mSceneAABB = scene.TotalAABB;

    // Load all object transforms and build object index cache:
    mObjectCache.clear();

    for (auto &[_, drawable] : mDrawables)
        drawable.Instances.clear();

    for (const auto &[objKey, obj] : scene.Objects)
    {
        if (!obj.Mesh.has_value())
            continue;

        auto meshKey = *obj.Mesh;

        for (const auto [primIdx, prim] : enumerate(scene.Meshes.at(meshKey).Primitives))
        {
            auto drawableKey = DrawableKey{meshKey, primIdx};

            if (mDrawables.count(drawableKey) == 0)
            {
                continue;
            }

            auto &drawable = mDrawables[drawableKey];

            auto &list = mObjectCache[objKey];
            list.emplace_back(drawableKey, drawable.Instances.size());

            glm::mat4 base = glm::translate(glm::mat4(1.0f), prim.BaseOffset) *
                             glm::scale(glm::mat4(1.0f), prim.BaseScale);

            glm::mat4 transform = obj.Transform * base;

            drawable.Instances.emplace_back(objKey, transform);
        }
    }
}