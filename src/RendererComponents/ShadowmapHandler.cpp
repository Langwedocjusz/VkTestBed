#include "ShadowmapHandler.h"

#include "Barrier.h"
#include "Common.h"
#include "Descriptor.h"
#include "ImageUtils.h"
#include "Sampler.h"
#include "VkInit.h"

#include <imgui_impl_vulkan.h>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/matrix.hpp>

#include <limits>

ShadowmapHandler::ShadowmapHandler(VulkanContext &ctx)
    : mCtx(ctx), mMainDeletionQueue(ctx), mPipelineDeletionQueue(ctx)
{
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

    // For debug view in imgui:
    mSamplerDebug = SamplerBuilder("MinimalPbrSampler2D")
                        .SetMagFilter(VK_FILTER_LINEAR)
                        .SetMinFilter(VK_FILTER_LINEAR)
                        .SetAddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT)
                        .SetMipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR)
                        .SetMaxLod(12.0f)
                        .Build(mCtx, mMainDeletionQueue);

    mDebugTextureDescriptorSet = ImGui_ImplVulkan_AddTexture(
        mSamplerDebug, mShadowmapView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

ShadowmapHandler::~ShadowmapHandler()
{
    mPipelineDeletionQueue.flush();
    mMainDeletionQueue.flush();
}

void ShadowmapHandler::RebuildPipelines(const Vertex::Layout &vertexLayout,
                                        VkDescriptorSetLayout materialDSLayout)
{
    mPipelineDeletionQueue.flush();

    mShadowmapPipeline = PipelineBuilder("MinimalPBRShadowmapPipeline")
                             .SetShaderPathVertex("assets/spirv/ShadowmapVert.spv")
                             .SetShaderPathFragment("assets/spirv/ShadowmapFrag.spv")
                             .SetVertexInput(vertexLayout, 0, VK_VERTEX_INPUT_RATE_VERTEX)
                             .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                             .SetPolygonMode(VK_POLYGON_MODE_FILL)
                             .SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE)
                             .RequestDynamicState(VK_DYNAMIC_STATE_CULL_MODE)
                             .SetPushConstantSize(sizeof(mShadowPCData))
                             .AddDescriptorSetLayout(materialDSLayout)
                             .EnableDepthTest()
                             .SetDepthFormat(mShadowmapFormat)
                             .Build(mCtx, mPipelineDeletionQueue);
}

void ShadowmapHandler::OnUpdate(Frustum camFr, glm::vec3 lightDir)
{
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

    mLightViewProj = lightProj * lightView;
}

void ShadowmapHandler::OnImGui()
{
    ImGui::SliderFloat("Add Z", &mAddZ, 0.0f, 10.0f);
    ImGui::SliderFloat("Sub Z", &mSubZ, 0.0f, 20.0f);
    ImGui::SliderFloat("Shadow Dist", &mShadowDist, 1.0f, 40.0f);

    ImGui::Image((ImTextureID)mDebugTextureDescriptorSet, ImVec2(512, 512));
}

void ShadowmapHandler::BeginShadowPass(VkCommandBuffer cmd)
{
    barrier::ImageBarrierDepthToRender(cmd, mShadowmap.Handle);

    auto extent = VkExtent2D(mShadowmap.Info.extent.width, mShadowmap.Info.extent.height);

    common::BeginRenderingDepth(cmd, extent, mShadowmapView, false, true);

    mShadowmapPipeline.Bind(cmd);
    common::ViewportScissor(cmd, extent);
}

void ShadowmapHandler::EndShadowPass(VkCommandBuffer cmd)
{
    vkCmdEndRendering(cmd);
    barrier::ImageBarrierDepthToSample(cmd, mShadowmap.Handle);
}

void ShadowmapHandler::PushConstantTransform(VkCommandBuffer cmd, glm::mat4 transform)
{
    mShadowPCData.LightMVP = mLightViewProj * transform;

    vkCmdPushConstants(cmd, mShadowmapPipeline.Layout, VK_SHADER_STAGE_ALL_GRAPHICS, 0,
                       sizeof(mShadowPCData), &mShadowPCData);
}

void ShadowmapHandler::BindMaterialDS(VkCommandBuffer cmd, VkDescriptorSet materialDS)
{
    mShadowmapPipeline.BindDescriptorSet(cmd, materialDS, 0);
}