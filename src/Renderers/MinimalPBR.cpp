#include "MinimalPBR.h"
#include "Pch.h"

#include "Camera.h"
#include "Common.h"
#include "Descriptor.h"
#include "GeometryData.h"
#include "ImGuiUtils.h"
#include "Pipeline.h"
#include "Renderer.h"
#include "Sampler.h"
#include "Scene.h"
#include "VkEnums.h"
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

// === Drawable Implementation: ======================================================

MinimalPbrRenderer::Drawable::Drawable(VulkanContext &ctx, const ScenePrimitive &prim, const std::string &debugName)
    : VertexBuffer(Buffer::MakeVertexStorage(ctx, debugName, prim.Data.VertexData))
    , VertexCount(static_cast<uint32_t>(prim.Data.VertexCount))
    , IndexBuffer(Buffer::MakeIndex(ctx, debugName, prim.Data.IndexData))
    , IndexCount(static_cast<uint32_t>(prim.Data.IndexCount))
    , Bbox(prim.Data.BBox)
    , TexBoundsCenter(prim.TexCoordCenter)
    , TexBoundsExtent(prim.TexCoordExtent)
{
    // Retrieve vertex buffer address:
    VkBufferDeviceAddressInfo deviceAdressInfo{
        .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext  = nullptr,
        .buffer = VertexBuffer.Handle,
    };

    VertexAddress = vkGetBufferDeviceAddress(ctx.Device, &deviceAdressInfo);
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

// === Renderer Implementation: ======================================================

MinimalPbrRenderer::MinimalPbrRenderer(VulkanContext &ctx, FrameInfo &info, Camera &camera)
    : IRenderer(ctx, info, camera)
    , mZPrepassOpaquePipeline(ctx)
    , mZPrepassAlphaPipeline(ctx)
    , mMainPipeline(ctx)
    , mStencilPipeline(ctx)
    , mOutlinePipeline(ctx)
    , mObjectIdPipeline(ctx)
    , mCamDynamicUBO(ctx, info, sizeof(mCamUBOData))
    , mDynamicUBO(ctx, info, sizeof(mUBOData))
    , mDynamicDS(ctx, info)
    , mMaterialDescriptorAllocator(ctx)
    , mSceneDeletionQueue(ctx)
    , mMaterialDeletionQueue(ctx)
{
    // Create the material texture sampler:
    // clang-format off
    mMaterialSampler = 
        SamplerBuilder("MinimalPbrMaterialSampler")
            .SetMagFilter(VK_FILTER_LINEAR)
            .SetMinFilter(VK_FILTER_LINEAR)
            .SetAddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT)
            .SetMipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR)
            .SetMaxLod(12.0f)
            .EnableMaxAnisotropy()
            .Build(mCtx, mMainDeletionQueue);
    // clang-format on

    // Create descriptor set layout for sampling material textures
    // and initialize the growable allocator:
    {
        // clang-format off
        mMaterialDescriptorSetLayout = 
            DescriptorSetLayoutBuilder("MinimalPBRMaterialDescriptorLayout")
                .AddCombinedSampler(0, VK_SHADER_STAGE_FRAGMENT_BIT)
                .AddCombinedSampler(1, VK_SHADER_STAGE_FRAGMENT_BIT)
                .AddCombinedSampler(2, VK_SHADER_STAGE_FRAGMENT_BIT)
                .AddUniformBuffer(3, VK_SHADER_STAGE_FRAGMENT_BIT)
                .Build(ctx, mMainDeletionQueue);
        // clang-format on

        mMaterialDescriptorAllocator.OnInit({
            .CombinedImageSampler = 3,
            .UniformBuffer        = 1,
        });
    }

    // Create the default textures:
    // clang-format off
    auto albedoData    = ImageData::SinglePixel(Pixel{255, 255, 255, 255}, false);
    auto roughnessData = ImageData::SinglePixel(Pixel{0,   255, 255,   0},  true);
    auto normalData    = ImageData::SinglePixel(Pixel{128, 128, 255,   0},  true);

    mDefaultAlbedo    = Texture::FromData(mCtx, "DefaultAlbedo",    albedoData);
    mDefaultRoughness = Texture::FromData(mCtx, "DefaultRoughness", roughnessData);
    mDefaultNormal    = Texture::FromData(mCtx, "DefaultNormal",    normalData);
    // clang-format on

    // Build the descriptor sets for the dynamic uniform buffer:
    {
        auto stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        // clang-format off
        auto layout = 
            DescriptorSetLayoutBuilder("MinimalPBRDynamicDescriptorSetLayout")
                .AddUniformBuffer(0, stageFlags)
                .AddUniformBuffer(1, stageFlags)
                .Build(mCtx, mMainDeletionQueue);
        // clang-format on

        auto counts = Descriptor::PoolCounts{
            .UniformBuffer = 2,
        };

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

    // Create static descriptor pool:
    auto poolSizes = Descriptor::PoolCounts{
        .DescriptorSet        = 20,
        .StorageImage         = 20,
        .CombinedImageSampler = 20,
        .UniformBuffer        = 20,
        .StorageBuffer        = 20,
    };

    mStaticDescriptorPool = Descriptor::InitPool(mCtx, poolSizes, mMainDeletionQueue);

    // Build the static descriptor set for ao/shadows etc:
    // clang-format off
    mAuxDescriptorSetLayout = 
        DescriptorSetLayoutBuilder("MinimalPBRShadowmapDescriptorLayout")
            .AddCombinedSampler(0, VK_SHADER_STAGE_FRAGMENT_BIT)
            .AddCombinedSampler(1, VK_SHADER_STAGE_FRAGMENT_BIT)
            .Build(ctx, mMainDeletionQueue);
    // clang-format on

    mAuxDescriptorSet = Descriptor::Allocate(mCtx, mStaticDescriptorPool, mAuxDescriptorSetLayout);

    // Initilize the subcomponents:
    EnvironmentHandler::BackgroundInfo envInfo{
        .ColorFormat = RenderTargetFormat,
        .DepthFormat = DepthStencilFormat,
        .SampleCount = mMultisample,
    };

    ShadowmapHandler::PipelineInfo shadowInfo{
        .VertexLayout         = mGeometryLayout.VertexLayout,
        .MaterialDSLayout     = mMaterialDescriptorSetLayout,
        .ColorFormat          = RenderTargetFormat,
        .DepthFormat          = DepthStencilFormat,
        .CurrentMultisampling = mMultisample,
    };

    mEnvHandler.emplace(ctx, mStaticDescriptorPool, envInfo);
    mShadowmapHandler.emplace(ctx, shadowInfo);
    mAOHandler.emplace(ctx, camera, mStaticDescriptorPool);
    mPostProcessor.emplace(ctx, mStaticDescriptorPool);

    // Build the owned graphics pipelines:
    RebuildOwnPipelines();
}

MinimalPbrRenderer::~MinimalPbrRenderer()
{
}

VkCompareOp MinimalPbrRenderer::GetMainCompareOp() const
{
    if (mEnablePrepass)
        return VK_COMPARE_OP_EQUAL;
    else
        return VK_COMPARE_OP_LESS;
}

void MinimalPbrRenderer::RebuildOwnPipelines()
{
    GraphicsPipelineBuilder(mZPrepassOpaquePipeline)
        .SetDebugName("MinimalPBROpaquePrepassPipeline")
        .SetShaderPathVertex("assets/spirv/ZPrepassOpaqueVert.spv")
        .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetPolygonMode(VK_POLYGON_MODE_FILL)
        .SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .RequestDynamicState(VK_DYNAMIC_STATE_CULL_MODE)
        .OverrideDescriptor(0, mDynamicDS.DescriptorSetLayout())
        .EnableDepthTest()
        .SetDepthFormat(DepthStencilFormat)
        .SetStencilFormat(DepthStencilFormat)
        .SetMultisampling(mMultisample)
        .Build(mCtx);

    GraphicsPipelineBuilder(mZPrepassAlphaPipeline)
        .SetDebugName("MinimalPBRAlphaPrepassPipeline")
        .SetShaderPathVertex("assets/spirv/ZPrepassAlphaVert.spv")
        .SetShaderPathFragment("assets/spirv/ZPrepassAlphaFrag.spv")
        .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetPolygonMode(VK_POLYGON_MODE_FILL)
        .SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .RequestDynamicState(VK_DYNAMIC_STATE_CULL_MODE)
        .OverrideDescriptor(0, mDynamicDS.DescriptorSetLayout())
        .OverrideDescriptor(1, mMaterialDescriptorSetLayout)
        .EnableDepthTest()
        .SetDepthFormat(DepthStencilFormat)
        .SetStencilFormat(DepthStencilFormat)
        .SetMultisampling(mMultisample)
        .Build(mCtx);

    GraphicsPipelineBuilder(mMainPipeline)
        .SetDebugName("MinimalPBRMainPipeline")
        .SetShaderPathVertex("assets/spirv/MinimalPBRVert.spv")
        .SetShaderPathFragment("assets/spirv/MinimalPBRFrag.spv")
        .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetPolygonMode(VK_POLYGON_MODE_FILL)
        .SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .EnableBlending()
        .RequestDynamicState(VK_DYNAMIC_STATE_CULL_MODE)
        .RequestDynamicState(VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT)
        .RequestDynamicState(VK_DYNAMIC_STATE_DEPTH_COMPARE_OP)
        .SetColorFormat(RenderTargetFormat)
        .OverrideDescriptor(0, mDynamicDS.DescriptorSetLayout())
        .OverrideDescriptor(1, mEnvHandler->GetLightingDSLayout())
        .OverrideDescriptor(2, mAuxDescriptorSetLayout)
        .OverrideDescriptor(3, mMaterialDescriptorSetLayout)
        .EnableDepthTest(GetMainCompareOp())
        .SetDepthFormat(DepthStencilFormat)
        .SetStencilFormat(DepthStencilFormat)
        .SetMultisampling(mMultisample)
        .Build(mCtx);

    VkStencilOpState stencilWriteState{
        .failOp      = VK_STENCIL_OP_REPLACE,
        .passOp      = VK_STENCIL_OP_REPLACE,
        .depthFailOp = VK_STENCIL_OP_REPLACE,
        .compareOp   = VK_COMPARE_OP_ALWAYS,
        .compareMask = ~0u,
        .writeMask   = ~0u,
        .reference   = 1,
    };

    GraphicsPipelineBuilder(mStencilPipeline)
        .SetDebugName("MinimalPBRStencilPipeline")
        .SetShaderPathVertex("assets/spirv/outline/StencilVert.spv")
        .SetShaderPathFragment("assets/spirv/outline/StencilFrag.spv")
        .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetPolygonMode(VK_POLYGON_MODE_FILL)
        .SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .EnableStencilTest(stencilWriteState, stencilWriteState)
        .SetStencilFormat(DepthStencilFormat)
        .EnableDepthTest(VK_COMPARE_OP_ALWAYS)
        .SetDepthFormat(DepthStencilFormat)
        .SetStencilFormat(DepthStencilFormat)
        .OverrideDescriptor(0, mDynamicDS.DescriptorSetLayout())
        .OverrideDescriptor(1, mMaterialDescriptorSetLayout)
        .SetMultisampling(mMultisample)
        .Build(mCtx);

    VkStencilOpState stencilOutlineState{
        .failOp      = VK_STENCIL_OP_KEEP,
        .passOp      = VK_STENCIL_OP_REPLACE,
        .depthFailOp = VK_STENCIL_OP_KEEP,
        .compareOp   = VK_COMPARE_OP_NOT_EQUAL,
        .compareMask = ~0u,
        .writeMask   = 0u,
        .reference   = 1,
    };

    GraphicsPipelineBuilder(mOutlinePipeline)
        .SetDebugName("MinimalPBROutlinePipeline")
        .SetShaderPathVertex("assets/spirv/outline/OutlineVert.spv")
        .SetShaderPathFragment("assets/spirv/outline/OutlineFrag.spv")
        .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetPolygonMode(VK_POLYGON_MODE_FILL)
        .SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .SetColorFormat(RenderTargetFormat)
        .EnableStencilTest(stencilOutlineState, stencilOutlineState)
        .SetStencilFormat(DepthStencilFormat)
        .EnableDepthTest(VK_COMPARE_OP_ALWAYS)
        .SetDepthFormat(DepthStencilFormat)
        .OverrideDescriptor(0, mDynamicDS.DescriptorSetLayout())
        .OverrideDescriptor(1, mMaterialDescriptorSetLayout)
        .SetMultisampling(mMultisample)
        .Build(mCtx);

    GraphicsPipelineBuilder(mObjectIdPipeline)
        .SetDebugName("MinimalPBRObjectIdPipeline")
        .SetShaderPathVertex("assets/spirv/outline/ObjectIdVert.spv")
        .SetShaderPathFragment("assets/spirv/outline/ObjectIdFrag.spv")
        .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetPolygonMode(VK_POLYGON_MODE_FILL)
        .SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .EnableDepthTest()
        .RequestDynamicState(VK_DYNAMIC_STATE_CULL_MODE)
        .SetColorFormat(PickingTargetFormat)
        .SetDepthFormat(PickingDepthFormat)
        .OverrideDescriptor(0, mDynamicDS.DescriptorSetLayout())
        .OverrideDescriptor(1, mMaterialDescriptorSetLayout)
        .Build(mCtx);
}

void MinimalPbrRenderer::RebuildPipelines()
{
    mPipelineDeletionQueue.flush();

    RebuildOwnPipelines();

    // Rebuild component pipelines as well:
    EnvironmentHandler::BackgroundInfo envInfo{
        .ColorFormat = RenderTargetFormat,
        .DepthFormat = DepthStencilFormat,
        .SampleCount = mMultisample,
    };

    ShadowmapHandler::PipelineInfo shadowInfo{
        .VertexLayout         = mGeometryLayout.VertexLayout,
        .MaterialDSLayout     = mMaterialDescriptorSetLayout,
        .ColorFormat          = RenderTargetFormat,
        .DepthFormat          = DepthStencilFormat,
        .CurrentMultisampling = mMultisample,
    };

    mEnvHandler->RebuildPipelines(envInfo);
    mShadowmapHandler->RebuildPipelines(shadowInfo);
    mAOHandler->RebuildPipelines();
    mPostProcessor->RebuildPipelines();
}

void MinimalPbrRenderer::RecreateSwapchainResources()
{
    using namespace vkenum::image_usage;

    mSwapchainDeletionQueue.flush();

    if (mMultisample == VK_SAMPLE_COUNT_1_BIT)
    {
        mRenderTargetMsaa = std::nullopt;
        mDepthStencilMsaa = std::nullopt;
    }

    // Create the render target:
    auto ScaleResolution = [this](uint32_t res) {
        return static_cast<uint32_t>(mInternalResolutionScale * static_cast<float>(res));
    };

    const VkExtent2D drawExtent{
        .width  = ScaleResolution(mCtx.Swapchain->Extent.width),
        .height = ScaleResolution(mCtx.Swapchain->Extent.height),
    };

    // Create the base render targets:
    // clang-format off
    mRenderTarget = Texture::Make2D(mCtx, {
        .Name   = "RenderTarget",
        .Extent = drawExtent,
        .Format = RenderTargetFormat,
        .Usage  = ColorAttachment | Sampled,
        .Layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    });

    mDepthStencilBuffer = Texture::Make2D(mCtx, {
        .Name   = "DepthBuffer",
        .Extent = drawExtent,
        .Format = DepthStencilFormat,
        .Usage  = DepthStencil | Sampled,
        .Layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    });

    // If multisampling used, create intermediate buffer, before resolve:
    if (mMultisample != VK_SAMPLE_COUNT_1_BIT)
    {
        mRenderTargetMsaa = Texture::Make2D(mCtx, {
            .Name          = "RenderTargetMSAA",
            .Extent        = drawExtent,
            .Format        = RenderTargetFormat,
            .Usage         = ColorAttachment | Sampled,
            .Layout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .Multisampling = mMultisample,
        });

        mDepthStencilMsaa = Texture::Make2D(mCtx, {
            .Name          = "DepthBufferMSAA",
            .Extent        = drawExtent,
            .Format        = DepthStencilFormat,
            .Usage         = DepthStencil | Sampled,
            .Layout        = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .Multisampling = mMultisample,
        });
    }
    // clang-format on

    // Create depth-only view (without stencil) for the depth buffer:
    mDepthOnlyView = mDepthStencilBuffer->Img.MakeView({
        .Name           = "DepthOnlyView",
        .AspectOverride = VK_IMAGE_ASPECT_DEPTH_BIT,
    });
    mSwapchainDeletionQueue.push_back(mDepthOnlyView);

    // (Re)Create AO related resources:
    mAOHandler->RecreateSwapchainResources(mDepthStencilBuffer->Img, mDepthOnlyView, drawExtent);

    // (Re)Create postfx related resources:
    mPostProcessor->RecreateSwapchainResources(mRenderTarget->Img, mRenderTarget->View);

    // Update auxiliary descriptor set:
    {
        auto [shadowView, shadowSampler] = mShadowmapHandler->GetViewAndSampler();
        auto [aoView, aoSampler]         = mAOHandler->GetViewAndSampler();

        DescriptorUpdater(mAuxDescriptorSet)
            .WriteCombinedSampler(0, shadowView, shadowSampler)
            .WriteCombinedSampler(1, aoView, aoSampler)
            .Update(mCtx);
    }
}

void MinimalPbrRenderer::OnUpdate([[maybe_unused]] float deltaTime)
{
    mShadowmapHandler->OnUpdate(mCamera, mEnvHandler->GetLightDir(), mSceneAABB);

    if (mAOHandler->RecreateRequested())
    {
        vkDeviceWaitIdle(mCtx.Device);
        mAOHandler->RecreateSwapchainResources();

        auto [aoView, aoSampler] = mAOHandler->GetViewAndSampler();

        DescriptorUpdater(mAuxDescriptorSet).WriteCombinedSampler(1, aoView, aoSampler).Update(mCtx);
    }

    glm::vec2 drawExtent{
        mRenderTarget->Img.Info.extent.width,
        mRenderTarget->Img.Info.extent.height,
    };

    // Update light/camera uniform buffer data:
    mCamUBOData.CameraViewProjection = mCamera.GetViewProj();
    mCamUBOData.ViewPos              = mCamera.GetPos();
    mCamUBOData.ViewFront            = mCamera.GetFront();

    mUBOData.ShadowMatrices   = mShadowmapHandler->GetMatrices();
    mUBOData.ShadowBounds     = mShadowmapHandler->GetBounds();
    mUBOData.ShadowTexelSizes = mShadowmapHandler->GetTexelSizes();
    mUBOData.AOEnabled        = mEnableAO;
    mUBOData.DrawExtent       = drawExtent;
}

void MinimalPbrRenderer::OnImGui()
{
    ImGui::Begin("Renderer settings");

    ImGui::Text("Num Single Sided: %zu", mSingleSidedDrawableKeys.size());
    ImGui::Text("Num Double Sided: %zu", mDoubleSidedDrawableKeys.size());
    ImGui::Text("Num Blended: %zu", mBlendedDrawableKeys.size());

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
    ImGui::SliderFloat("Environment Saturation", &mUBOData.EnvSaturation, 0.0f, 2.0f);

    ImGui::SliderFloat("Shadow Bias Light", &mUBOData.ShadowBiasLight, 0.0f, 3.0f);
    ImGui::SliderFloat("Shadow Bias Normal", &mUBOData.ShadowBiasNormal, 0.0f, 3.0f);

    if (ImGui::CollapsingHeader("PostFX"))
    {
        mPostProcessor->OnImGui();
    }

    if (ImGui::CollapsingHeader("Render Target"))
    {
        ImGui::SliderFloat("Internal Res Scale", &mInternalResolutionScale, 0.25f, 2.0f);

        static int        choice;
        static std::array options{"1x", "2x", "4x", "8x"};

        imutils::Combo("Multisampling", choice, options);

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
        mShadowmapHandler->OnImGui();

    if (ImGui::CollapsingHeader("Ambient Occlusion"))
        mAOHandler->OnImGui();

    ImGui::End();
}

void MinimalPbrRenderer::OnRender([[maybe_unused]] std::optional<SceneKey> highlightedObj)
{
    auto &cmd = mFrame.CurrentCmd();

    // This is not OnUpdate, since uniform buffers are per-image index
    // and as such need to be acquired after new image index is set.
    mCamDynamicUBO.UpdateData(&mCamUBOData, sizeof(mCamUBOData));
    mDynamicUBO.UpdateData(&mUBOData, sizeof(mUBOData));

    DrawStats stats{};

    ShadowPass(cmd, stats);

    if (mEnablePrepass)
    {
        Prepass(cmd, stats);

        if (mEnableAO)
        {
            bool msaa = (mMultisample != VK_SAMPLE_COUNT_1_BIT);

            barrier::Depth2ToSampledComp(cmd, mDepthStencilBuffer->Img, msaa);
            mAOHandler->RunAOPass(cmd);
            barrier::Depth2ToRenderAfterComp(cmd, mDepthStencilBuffer->Img, msaa);
        }
    }

    barrier::SampledToColor(cmd, mRenderTarget->Img.Handle);
    MainPass(cmd, stats);

    // TODO: This should be moved after postfx
    // and write to the final target:
    if (highlightedObj.has_value())
        OutlinePass(cmd, *highlightedObj);

    barrier::ColorToSampledComp(cmd, mRenderTarget->Img.Handle);
    mPostProcessor->RunPostProcessPass(cmd);

    mFrame.Stats.NumTriangles = stats.NumIdx / 3;
    mFrame.Stats.NumDraws     = stats.NumDraws;
    mFrame.Stats.NumBinds     = stats.NumBinds;
}

void MinimalPbrRenderer::TargetToTransfer(VkCommandBuffer cmd)
{
    // barrier::ColorToTransferSrc(cmd, mRenderTarget.Img.Handle);
    (void)cmd;
}

template <typename MaterialFn, typename InstanceFn>
void MinimalPbrRenderer::DrawAllInstancesCulled(
    VkCommandBuffer cmd,
    Drawable       &drawable,
    glm::mat4       viewProj,
    MaterialFn      materialCallback,
    InstanceFn      instanceCallback,
    DrawStats      &stats
)
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
void MinimalPbrRenderer::DrawSingleSidedFrustumCulled(
    VkCommandBuffer cmd, glm::mat4 viewProj, MaterialFn materialCallback, InstanceFn instanceCallback, DrawStats &stats
)
{
    vkCmdSetCullMode(cmd, VK_CULL_MODE_BACK_BIT);

    for (auto key : mSingleSidedDrawableKeys)
    {
        auto &drawable = mDrawables.at(key);

        DrawAllInstancesCulled(cmd, drawable, viewProj, materialCallback, instanceCallback, stats);
    }
}

template <typename MaterialFn, typename InstanceFn>
void MinimalPbrRenderer::DrawDoubleSidedFrustumCulled(
    VkCommandBuffer cmd, glm::mat4 viewProj, MaterialFn materialCallback, InstanceFn instanceCallback, DrawStats &stats
)
{
    vkCmdSetCullMode(cmd, VK_CULL_MODE_NONE);

    for (auto key : mDoubleSidedDrawableKeys)
    {
        auto &drawable = mDrawables.at(key);

        DrawAllInstancesCulled(cmd, drawable, viewProj, materialCallback, instanceCallback, stats);
    }
}

template <typename MaterialFn, typename InstanceFn>
void MinimalPbrRenderer::DrawBlendedFrustumCulled(
    VkCommandBuffer cmd, glm::mat4 viewProj, MaterialFn materialCallback, InstanceFn instanceCallback, DrawStats &stats
)
{
    vkCmdSetCullMode(cmd, VK_CULL_MODE_BACK_BIT);

    for (auto key : mBlendedDrawableKeys)
    {
        auto &drawable = mDrawables.at(key);
        DrawAllInstancesCulled(cmd, drawable, viewProj, materialCallback, instanceCallback, stats);
    }
}

void MinimalPbrRenderer::ShadowPass(VkCommandBuffer cmd, DrawStats &stats)
{
    auto drawOpaque = [&](VkCommandBuffer cmd, glm::mat4 viewProj) {
        auto materialCallback = [](VkCommandBuffer cmd, Material &material) {
            (void)cmd;
            (void)material;
        };

        auto instanceCallback = [&](VkCommandBuffer cmd, Drawable &drawable, Instance &instance) {
            ShadowmapHandler::PCData data{
                .LightMVP       = viewProj * instance.TransformRaw,
                .VertexBuffer   = drawable.VertexAddress,
                .TexBoundCenter = drawable.TexBoundsCenter,
                .TexBoundExtent = drawable.TexBoundsExtent,
            };

            mShadowmapHandler->PushConstantOpaque(cmd, data);
        };

        DrawSingleSidedFrustumCulled(cmd, viewProj, materialCallback, instanceCallback, stats);
    };

    auto drawAlpha = [&](VkCommandBuffer cmd, glm::mat4 viewProj) {
        auto materialCallback = [&](VkCommandBuffer cmd, Material &material) {
            mShadowmapHandler->BindAlphaMaterialDS(cmd, material.DescriptorSet);
            stats.NumBinds += 1;
        };

        auto instanceCallback = [&](VkCommandBuffer cmd, Drawable &drawable, Instance &instance) {
            ShadowmapHandler::PCData data{
                .LightMVP       = viewProj * instance.TransformRaw,
                .VertexBuffer   = drawable.VertexAddress,
                .TexBoundCenter = drawable.TexBoundsCenter,
                .TexBoundExtent = drawable.TexBoundsExtent,
            };

            mShadowmapHandler->PushConstantAlpha(cmd, data);
        };

        DrawDoubleSidedFrustumCulled(cmd, viewProj, materialCallback, instanceCallback, stats);
    };

    mShadowmapHandler->DrawShadowmaps(cmd, drawOpaque, drawAlpha);
}

void MinimalPbrRenderer::Prepass(VkCommandBuffer cmd, DrawStats &stats)
{
    auto renderInfo = common::RenderingInfo{
        .Extent          = GetTargetSize(),
        .Depth           = mDepthStencilBuffer->View,
        .DepthHasStencil = true,
    };

    if (mMultisample != VK_SAMPLE_COUNT_1_BIT)
    {
        renderInfo.Depth        = mDepthStencilMsaa->View;
        renderInfo.DepthResolve = mDepthStencilBuffer->View;
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

        auto instanceCallback = [this](VkCommandBuffer cmd, Drawable &drawable, Instance &instance) {
            struct PCData {
                glm::mat4       Model;
                VkDeviceAddress VertexBuffer;
            };

            // clang-format off
            mZPrepassOpaquePipeline.PushConstants(cmd, PCData{
                .Model        = instance.TransformRaw,
                .VertexBuffer = drawable.VertexAddress,
            });
            // clang-format on
        };

        DrawSingleSidedFrustumCulled(cmd, viewProj, materialCallback, instanceCallback, stats);
    }

    {
        mZPrepassAlphaPipeline.Bind(cmd);
        common::ViewportScissor(cmd, GetTargetSize());

        mZPrepassAlphaPipeline.BindDescriptorSet(cmd, mDynamicDS.DescriptorSet(), 0);

        auto materialCallback = [this, &stats](VkCommandBuffer cmd, Material &material) {
            mZPrepassAlphaPipeline.BindDescriptorSet(cmd, material.DescriptorSet, 1);
            stats.NumBinds += 1;
        };

        auto instanceCallback = [this](VkCommandBuffer cmd, Drawable &drawable, Instance &instance) {
            struct PCData {
                glm::mat4       Model;
                VkDeviceAddress VertexBuffer;
                glm::vec2       TexBoundCenter;
                glm::vec2       TexBoundExtent;
            };
            // clang-format off
            mZPrepassAlphaPipeline.PushConstants(cmd, PCData{
                .Model          = instance.TransformRaw,
                .VertexBuffer   = drawable.VertexAddress,
                .TexBoundCenter = drawable.TexBoundsCenter,
                .TexBoundExtent = drawable.TexBoundsExtent,
            });
            // clang-format on
        };

        DrawDoubleSidedFrustumCulled(cmd, viewProj, materialCallback, instanceCallback, stats);
    }

    vkCmdEndRendering(cmd);
}

void MinimalPbrRenderer::MainPass(VkCommandBuffer cmd, DrawStats &stats)
{
    auto renderInfo = common::RenderingInfo{
        .Extent          = GetTargetSize(),
        .Color           = mRenderTarget->View,
        .Depth           = mDepthStencilBuffer->View,
        .DepthHasStencil = true,
    };

    if (mEnablePrepass)
        renderInfo.ClearDepth = std::nullopt;

    if (mMultisample != VK_SAMPLE_COUNT_1_BIT)
    {
        renderInfo.Color        = mRenderTargetMsaa->View;
        renderInfo.Depth        = mDepthStencilMsaa->View;
        renderInfo.ColorResolve = mRenderTarget->View;
        renderInfo.DepthResolve = mDepthStencilBuffer->View;
    }

    common::BeginRendering(cmd, renderInfo);

    // Draw the scene:
    mMainPipeline.Bind(cmd);
    common::ViewportScissor(cmd, GetTargetSize());

    std::array descriptorSets{
        mDynamicDS.DescriptorSet(),
        mEnvHandler->GetLightingDS(),
        mAuxDescriptorSet,
    };

    mMainPipeline.BindDescriptorSets(cmd, descriptorSets, 0);
    stats.NumBinds += 3;

    auto viewProj = mCamUBOData.CameraViewProjection;

    auto materialCallback = [this, &stats](VkCommandBuffer cmd, Material &material) {
        mMainPipeline.BindDescriptorSet(cmd, material.DescriptorSet, 3);
        stats.NumBinds += 1;
    };

    auto instanceCallback = [this](VkCommandBuffer cmd, Drawable &drawable, Instance &instance) {
        struct PCData {
            glm::mat4       Model;
            VkDeviceAddress VertexBuffer;
            glm::vec2       TexBoundCenter;
            glm::vec2       TexBoundExtent;
        };
        // clang-format off
        mMainPipeline.PushConstants(cmd, PCData{
            .Model          = instance.TransformRaw,
            .VertexBuffer   = drawable.VertexAddress,
            .TexBoundCenter = drawable.TexBoundsCenter,
            .TexBoundExtent = drawable.TexBoundsExtent,
        });
        // clang-format on
    };

    {
        // Disable blending, set depth op to default:
        VkBool32 blendEnables[] = {VK_FALSE};
        vkCmdSetColorBlendEnableEXT(cmd, 0, 1, blendEnables);
        vkCmdSetDepthCompareOp(cmd, GetMainCompareOp());
    }

    DrawSingleSidedFrustumCulled(cmd, viewProj, materialCallback, instanceCallback, stats);
    DrawDoubleSidedFrustumCulled(cmd, viewProj, materialCallback, instanceCallback, stats);

    {
        // Enable blending, set depth op to less:
        VkBool32 blendEnables[] = {VK_TRUE};
        vkCmdSetColorBlendEnableEXT(cmd, 0, 1, blendEnables);
        vkCmdSetDepthCompareOp(cmd, VK_COMPARE_OP_LESS);
    }

    // TODO: Add sorting by depth:
    DrawBlendedFrustumCulled(cmd, viewProj, materialCallback, instanceCallback, stats);

    // At this point debug visuals from the shadow map can optionally be drawn:
    // TODO: This is kind of ugly, but debug needs to have acces to the main depth
    // buffer. Think about a cleaner way to inject debug visualization rendering.
    mShadowmapHandler->DrawDebugShapes(cmd, mCamUBOData.CameraViewProjection, GetTargetSize());

    // Draw the background:
    auto frustumBack = mCamera.GetFrustumBack();
    mEnvHandler->DrawBackground(cmd, frustumBack, GetTargetSize());

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

    // Push Constants layout for outline pass:
    struct PCData {
        glm::mat4       Model;
        VkDeviceAddress VertexBuffer;
        glm::vec2       TexBoundCenter;
        glm::vec2       TexBoundExtent;
    };

    // Draw to stencil:
    // TODO: this does no check to see if the current drawable is double sided
    // and wheter or not the vkCullState is set accordingly.
    {
        auto renderInfo = common::RenderingInfo{
            .Extent          = GetTargetSize(),
            .Depth           = mDepthStencilBuffer->View,
            .DepthHasStencil = true,
            .ClearDepth      = std::nullopt,
        };

        if (mMultisample != VK_SAMPLE_COUNT_1_BIT)
        {
            renderInfo.Depth        = mDepthStencilMsaa->View;
            renderInfo.DepthResolve = mDepthStencilBuffer->View;
        }

        common::BeginRendering(cmd, renderInfo);

        mStencilPipeline.Bind(cmd);
        common::ViewportScissor(cmd, GetTargetSize());

        mStencilPipeline.BindDescriptorSet(cmd, mDynamicDS.DescriptorSet(), 0);

        for (auto [drawableKey, instanceId] : mSelectedDrawableKeys)
        {
            auto &drawable = mDrawables.at(drawableKey);

            // Do frustum culling:
            if (!drawable.IsVisible(mCamUBOData.CameraViewProjection, instanceId))
                break;

            // Bind all per-drawable resources:
            drawable.BindGeometryBuffers(cmd);

            auto &material = mMaterials.at(drawable.MaterialKey);
            mStencilPipeline.BindDescriptorSet(cmd, material.DescriptorSet, 1);

            // Push per-instance data:
            auto &model = drawable.Instances.at(instanceId).TransformRaw;

            // clang-format off
            mStencilPipeline.PushConstants(cmd, PCData{
                .Model          = model,
                .VertexBuffer   = drawable.VertexAddress,
                .TexBoundCenter = drawable.TexBoundsCenter,
                .TexBoundExtent = drawable.TexBoundsExtent,
            });
            // clang-format on

            // Draw:
            drawable.Draw(cmd);
        }

        vkCmdEndRendering(cmd);
    }

    // Draw outline:
    {
        auto renderInfo = common::RenderingInfo{
            .Extent          = GetTargetSize(),
            .Color           = mRenderTarget->View,
            .Depth           = mDepthStencilBuffer->View,
            .DepthHasStencil = true,
            .ClearColor      = std::nullopt,
            .ClearDepth      = std::nullopt,
        };

        if (mMultisample != VK_SAMPLE_COUNT_1_BIT)
        {
            renderInfo.Color        = mRenderTargetMsaa->View;
            renderInfo.Depth        = mDepthStencilMsaa->View;
            renderInfo.ColorResolve = mRenderTarget->View;
            renderInfo.DepthResolve = mDepthStencilBuffer->View;
        }

        common::BeginRendering(cmd, renderInfo);

        mOutlinePipeline.Bind(cmd);
        common::ViewportScissor(cmd, GetTargetSize());

        mOutlinePipeline.BindDescriptorSet(cmd, mDynamicDS.DescriptorSet(), 0);

        for (auto [drawableKey, instanceId] : mSelectedDrawableKeys)
        {
            auto &drawable = mDrawables.at(drawableKey);

            // Do frustum culling:
            if (!drawable.IsVisible(mCamUBOData.CameraViewProjection, instanceId))
                break;

            // Bind all per-drawable resources:
            drawable.BindGeometryBuffers(cmd);

            auto &material = mMaterials.at(drawable.MaterialKey);
            mOutlinePipeline.BindDescriptorSet(cmd, material.DescriptorSet, 1);

            // Push per-instance data:
            auto &model = drawable.Instances.at(instanceId).TransformRaw;

            // clang-format off
            mOutlinePipeline.PushConstants(cmd, PCData{
                .Model          = model,
                .VertexBuffer   = drawable.VertexAddress,
                .TexBoundCenter = drawable.TexBoundsCenter,
                .TexBoundExtent = drawable.TexBoundsExtent,
            });
            // clang-format on

            // Draw:
            drawable.Draw(cmd);
        }

        vkCmdEndRendering(cmd);
    }
}

void MinimalPbrRenderer::RenderObjectId(VkCommandBuffer cmd, float x, float y)
{
    // Calculate camera matrix:
    float pixel_dx = 1.0f / static_cast<float>(mRenderTarget->Img.Info.extent.width);
    float pixel_dy = 1.0f / static_cast<float>(mRenderTarget->Img.Info.extent.height);

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

    auto instanceCallback = [this, viewProj](VkCommandBuffer cmd, Drawable &drawable, Instance &instance) {
        struct PCData {
            glm::mat4       Model;
            VkDeviceAddress VertexBuffer;
            glm::vec2       TexBoundCenter;
            glm::vec2       TexBoundExtent;
            uint32_t        ObjectId;
        };
        // clang-format off
        mObjectIdPipeline.PushConstants(cmd, PCData{
            .Model          = viewProj * instance.TransformRaw,
            .VertexBuffer   = drawable.VertexAddress,
            .TexBoundCenter = drawable.TexBoundsCenter,
            .TexBoundExtent = drawable.TexBoundsExtent,
            .ObjectId       = instance.ObjectId,
        });
        // clang-format on
    };

    DrawStats stats;

    DrawSingleSidedFrustumCulled(cmd, viewProj, materialCallback, instanceCallback, stats);
    DrawDoubleSidedFrustumCulled(cmd, viewProj, materialCallback, instanceCallback, stats);
    DrawBlendedFrustumCulled(cmd, viewProj, materialCallback, instanceCallback, stats);
}

void MinimalPbrRenderer::LoadScene(const Scene &scene)
{
    if (scene.FullReloadRequested())
    {
        mDrawables.clear();
        mMaterials.clear();

        for (auto &[_, texture] : mTextures)
            DestroyTexture(texture);

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
        mEnvHandler->LoadEnvironment(scene);
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
            if (mDrawables.contains(drawableKey))
                continue;

            // Wrong layout - skip:
            if (prim.Data.Layout != mGeometryLayout)
                continue;

            // Create new drawable:
            const auto debugName = mesh.Name + std::to_string(primIdx);

            Drawable drawable{mCtx, prim, debugName};

            mDrawables.try_emplace(drawableKey, std::move(drawable));
        }
    }

    // Prune orphaned drawables:
    std::erase_if(mDrawables, [&](auto &item) {
        auto &[key, _drawable] = item;

        auto meshKey = key.first;
        bool erase   = scene.Meshes.count(meshKey) == 0;

        return erase;
    });
}

void MinimalPbrRenderer::LoadImages(const Scene &scene)
{
    for (auto &[key, imgData] : scene.Images)
    {
        const bool alreadyLoaded = mTextures.contains(key);

        if (alreadyLoaded && imgData.IsUpToDate)
            continue;

        mTextures.insert_or_assign(key, Texture::FromData(mCtx, "MaterialTexture", imgData));
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
        const bool firstLoad = !mMaterials.contains(key);

        // Only allocate new descriptor set on first load:
        if (firstLoad)
        {
            Buffer::ThinInfo uboInfo{
                .Name = "Material " + sceneMat.Name + "UBO",
                .Size = sizeof(MaterialUboData),
            };

            // clang-format off
            mMaterials.try_emplace(key, Material{
                .DescriptorSet = mMaterialDescriptorAllocator.Allocate(mMaterialDescriptorSetLayout),
                .UBO           = Buffer::MakeMappedUniform(mCtx, uboInfo),
                .UboData       = {},
            });
            // clang-format on
        }

        auto &mat = mMaterials.at(key);

        // Update the non-image parameters:
        mat.UboData.DoubleSided = sceneMat.DoubleSided;
        mat.UboData.AlphaMode   = sceneMat.AlphaMode;
        mat.UboData.AlphaCutoff = sceneMat.AlphaCutoff;

        if (sceneMat.TranslucentColor.has_value())
            mat.UboData.TranslucentColor = *sceneMat.TranslucentColor;

        mat.UBO.UploadMapped(&mat.UboData, sizeof(mat.UboData));

        // Retrieve the textures if available:
        auto GetTexture = [&](std::optional<SceneKey> opt, Texture &def) -> Texture & {
            if (opt.has_value())
            {
                if (mTextures.contains(*opt))
                    return mTextures.at(*opt);
            }

            return def;
        };

        auto &albedo    = GetTexture(sceneMat.Albedo, *mDefaultAlbedo);
        auto &roughness = GetTexture(sceneMat.Roughness, *mDefaultRoughness);
        auto &normal    = GetTexture(sceneMat.Normal, *mDefaultNormal);

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
    mBlendedDrawableKeys.clear();

    for (const auto &[meshKey, mesh] : scene.Meshes)
    {
        for (const auto [primIdx, prim] : enumerate(mesh.Primitives))
        {
            auto drawableKey = DrawableKey{meshKey, primIdx};

            if (mDrawables.contains(drawableKey))
            {
                auto  matKey = *prim.Material;
                auto &mat    = mMaterials.at(matKey);

                auto &drawable = mDrawables.at(drawableKey);

                if (prim.Material)
                    drawable.MaterialKey = matKey;

                if (mat.UboData.AlphaMode == MaterialAlphaMode::Blend)
                    mBlendedDrawableKeys.push_back(drawableKey);
                else
                {
                    if (mat.UboData.DoubleSided)
                        mDoubleSidedDrawableKeys.push_back(drawableKey);
                    else
                        mSingleSidedDrawableKeys.push_back(drawableKey);
                }
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

            if (!mDrawables.contains(drawableKey))
            {
                continue;
            }

            auto &drawable = mDrawables.at(drawableKey);

            auto &list = mObjectCache[objKey];
            list.emplace_back(drawableKey, drawable.Instances.size());

            glm::mat4 base =
                glm::translate(glm::mat4(1.0f), prim.BaseOffset) * glm::scale(glm::mat4(1.0f), prim.BaseScale);

            drawable.Instances.emplace_back(objKey, obj.Transform, obj.Transform * base);
        }
    }
}