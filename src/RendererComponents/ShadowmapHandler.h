#pragma once

#include "Barrier.h"
#include "Camera.h"
#include "Common.h"
#include "DeletionQueue.h"
#include "GeometryData.h"
#include "Pipeline.h"
#include "VertexLayout.h"
#include "VulkanContext.h"

#include "volk.h"
#include <glm/glm.hpp>

#include <optional>

struct ShadowVolume {
    float MinX;
    float MaxX;
    float MinY;
    float MaxY;
    float MinZ;
    float MaxZ;
};

class ShadowmapHandler {
  public:
    static constexpr size_t NumCascades = 3;

    using Matrices   = std::array<glm::mat4, NumCascades>;
    using Bounds     = std::array<float, NumCascades>;
    using TexelSizes = std::array<float, NumCascades>;

  public:
    ShadowmapHandler(VulkanContext &ctx);

    void OnUpdate(Camera &camera, glm::vec3 lightDir, AABB sceneAABB);
    void OnImGui();

    // Information about main rendering pipeline using this handler:
    struct PipelineInfo {
        Vertex::Layout        VertexLayout;
        VkDescriptorSetLayout MaterialDSLayout;
        VkFormat              ColorFormat;
        VkFormat              DepthFormat;
        VkSampleCountFlagBits CurrentMultisampling;
    };

    void RebuildPipelines(const PipelineInfo &info);

    // Draw into all shadowmap cascades using user-provided drawing functions:
    template <typename OpaqueFn, typename AlphaFn>
    void DrawShadowmaps(VkCommandBuffer cmd, OpaqueFn drawOpaque, AlphaFn drawAlpha);

    // For building drawing functions in the renderer:

    // Push-constant struct for shadowmap-rendering pipelines:
    // Includes (pre-multiplied by model) MVP matrix,
    // device address of the vertex buffer (vertex pulling mode),
    // and texture bounds of the given material, needed for
    // alpha testing.
    struct PCData {
        glm::mat4       LightMVP;
        VkDeviceAddress VertexBuffer;
        glm::vec2       TexBoundCenter;
        glm::vec2       TexBoundExtent;
    };

    // This variant is for fully opaque geometry:
    void PushConstantOpaque(VkCommandBuffer cmd, PCData &data);
    // And this one is for alpha tested geometry (like foliage):
    void PushConstantAlpha(VkCommandBuffer cmd, PCData &data);

    // Binds descriptor set used to sample per-material alpha.
    // Descriptor set must be consistend with the materialDSLayout provided at
    // construction. Descriptor set being bound is assumed to have albedo map as its first
    // binding. It is also assumed that transparency is stored in the 'a' channel.
    void BindAlphaMaterialDS(VkCommandBuffer cmd, VkDescriptorSet materialDS);

    // Retrieve data needed to access the shadow maps:
    [[nodiscard]] std::pair<VkImageView, VkSampler> GetViewAndSampler() const
    {
        return {mShadowmap.View, mSampler};
    }

    // Retrieve other data nedeed when using the shadow maps:
    [[nodiscard]] Matrices GetMatrices() const
    {
        return mMatrices;
    }
    [[nodiscard]] Bounds GetBounds() const
    {
        return mBounds;
    }
    [[nodiscard]] TexelSizes GetTexelSizes() const
    {
        return mTexelSizes;
    }

    // Prerform debug visualization of the shadow casting frustums:
    void DrawDebugShapes(VkCommandBuffer cmd, glm::mat4 viewProj, VkExtent2D extent);

  private:
    [[nodiscard]] VkExtent2D GetExtent() const;

    [[nodiscard]] Frustum ScaleCameraFrustum(Frustum camFrustum, float distNear,
                                             float distFar) const;
    [[nodiscard]] std::pair<ShadowVolume, float> GetBoundingVolume(
        Frustum shadowFrustum, glm::mat4 lightView) const;
    /// Scene AABB is take to be in world coords
    [[nodiscard]] ShadowVolume FitVolumeToScene(ShadowVolume volume, AABB sceneAABB,
                                                glm::mat4 lightView) const;

  private:
    static constexpr uint32_t ShadowmapResolution = 2048;
    static constexpr VkFormat ShadowmapFormat     = VK_FORMAT_D32_SFLOAT;

    VulkanContext              &mCtx;
    std::optional<PipelineInfo> mPipelineInfo = std::nullopt;

    // Configurable parameters:
    bool mDebugView     = false;
    bool mFreezeFrustum = false;
    bool mFitToScene    = true;

    float mShadowDist = 10.0f;

    // The shadowmap-rendering pipelines:
    Pipeline mOpaquePipeline;
    Pipeline mAlphaPipeline;

    // Main (multi layer) shadowmap texture and corresponding sampler:
    Texture   mShadowmap;
    VkSampler mSampler;

    // Single layer views for rendering subsequent cascades:
    std::array<VkImageView, NumCascades> mCascadeViews;

    // Data that needs to be uploaded via uniform buffer
    // to use shadowmaps in actual rendering:
    Matrices   mMatrices;
    Bounds     mBounds;
    TexelSizes mTexelSizes;

    // Parts of the camera frustum corresponding to subsequent cascades.
    // Used to determine their projection matrices:
    std::array<Frustum, NumCascades> mShadowFrustums;

    // Descriptor set and sampler for sending shadow map view to imgui:
    VkDescriptorSet mDebugTextureDescriptorSet;
    VkSampler       mDebugSampler;

    // Additional pipeline and resources for debug visualization:
    Pipeline mDebugPipeline;

    struct PCDataDebug {
        glm::mat4 ViewProj;
        glm::vec4 Color;
    };

    // Debug frustum geometry data:
    static constexpr size_t NumVertsPerFrustum = 8;
    static constexpr size_t NumIdxPerFrustum   = 36;

    std::array<glm::vec3, 2 * NumVertsPerFrustum> mVertexBufferData;

    GeometryLayout mDebugGeometryLayout{
        .VertexLayout = Vertex::PushLayout{},
        .IndexType    = VK_INDEX_TYPE_UINT16,
    };

    Buffer mDebugFrustumVertexBuffer;
    Buffer mDebugFrustumIndexBuffer;

    DeletionQueue mMainDeletionQueue;
    DeletionQueue mPipelineDeletionQueue;
};

template <typename OpaqueFn, typename AlphaFn>
void ShadowmapHandler::DrawShadowmaps(VkCommandBuffer cmd, OpaqueFn drawOpaque,
                                      AlphaFn drawAlpha)
{
    barrier::DepthToRender(cmd, mShadowmap.Img.Handle, NumCascades);

    // TODO: for now we are just issuing render commands 3 times - for each cascade.
    // This can be optimized to one render pass, for example with usage of the
    // multiview extension.
    for (size_t idx = 0; idx < NumCascades; idx++)
    {
        // Draw functions are fed the view-proj matrices
        // so they can use them to do frustum culling.
        auto viewProj = mMatrices[idx];

        auto info = common::RenderingInfo{
            .Extent          = GetExtent(),
            .Depth           = mCascadeViews[idx],
            .DepthHasStencil = false,
        };
        common::BeginRendering(cmd, info);

        mOpaquePipeline.Bind(cmd);
        common::ViewportScissor(cmd, GetExtent());
        drawOpaque(cmd, viewProj);

        mAlphaPipeline.Bind(cmd);
        common::ViewportScissor(cmd, GetExtent());
        drawAlpha(cmd, viewProj);

        vkCmdEndRendering(cmd);
    }

    barrier::DepthToSample(cmd, mShadowmap.Img.Handle, NumCascades);
}