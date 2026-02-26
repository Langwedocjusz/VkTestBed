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

    using Matrices = std::array<glm::mat4, NumCascades>;
    using Bounds   = std::array<float, NumCascades>;

  public:
    ShadowmapHandler(VulkanContext &ctx, VkFormat debugColorFormat,
                     VkFormat debugDepthFormat);
    ~ShadowmapHandler();

    void OnUpdate(Frustum camFr, glm::vec3 frontDir, glm::vec3 lightDir, AABB sceneAABB);
    void OnImGui();

    void RebuildPipelines(const Vertex::Layout &vertexLayout,
                          VkDescriptorSetLayout materialDSLayout,
                          VkSampleCountFlagBits debugMultisampling);

    // Draw all shadowmap cascades using user-provided drawing functions:
    template <typename OpaqueFn, typename AlphaFn>
    void DrawShadowmaps(VkCommandBuffer cmd, OpaqueFn drawOpaque, AlphaFn drawAlpha)
    {
        barrier::ImageBarrierDepthToRender(cmd, mShadowmap.Img.Handle, NumCascades);

        // TODO: for now we are just issuing render commands 3 times - for each cascade.
        // This can be optimized to one render pass, for example with usage of the
        // multiview extension.
        for (size_t idx = 0; idx < NumCascades; idx++)
        {
            auto viewProj = mLightViewProjs[idx];

            common::BeginRenderingDepth(cmd, GetExtent(), mCascadeViews[idx], false,
                                        true);

            mOpaquePipeline.Bind(cmd);
            common::ViewportScissor(cmd, GetExtent());
            drawOpaque(cmd, viewProj);

            mAlphaPipeline.Bind(cmd);
            common::ViewportScissor(cmd, GetExtent());
            drawAlpha(cmd, viewProj);

            vkCmdEndRendering(cmd);
        }

        barrier::ImageBarrierDepthToSample(cmd, mShadowmap.Img.Handle, NumCascades);
    }

    // For building drawing functions in the renderer:

    // Deliver per-object (pre-multiplied) MVP matrix to the shaders via push constant:
    void PushConstantOpaque(VkCommandBuffer cmd, glm::mat4 mvp, VkDeviceAddress vertexBuffer);
    void PushConstantAlpha(VkCommandBuffer cmd, glm::mat4 mvp, VkDeviceAddress vertexBuffer);

    // Bind descriptor set used to sample per-material alpha.
    // Descriptor set being bound is assumed to have albedo map as its first binding.
    // It is also assumed that transparency is stored in the 'a' channel.
    void BindAlphaMaterialDS(VkCommandBuffer cmd, VkDescriptorSet materialDS);

    [[nodiscard]] VkDescriptorSetLayout GetDSLayout() const
    {
        return mShadowmapDescriptorSetLayout;
    }
    [[nodiscard]] VkDescriptorSet GetDescriptorSet() const
    {
        return mShadowmapDescriptorSet;
    }
    [[nodiscard]] Matrices GetViewProj() const
    {
        return mLightViewProjs;
    }
    [[nodiscard]] Bounds GetBounds() const
    {
        return mBounds;
    }

    void DrawDebugShapes(VkCommandBuffer cmd, glm::mat4 viewProj, VkExtent2D extent);

  private:
    [[nodiscard]] VkExtent2D GetExtent() const;

    [[nodiscard]] Frustum      ScaleCameraFrustum(Frustum camFrustum, float distNear,
                                                  float distFar) const;
    [[nodiscard]] ShadowVolume GetBoundingVolume(Frustum   shadowFrustum,
                                                 glm::mat4 lightView) const;
    /// Scene AABB is take to be in world coords
    [[nodiscard]] ShadowVolume FitVolumeToScene(ShadowVolume volume, AABB sceneAABB,
                                                glm::mat4 lightView) const;

  private:
    static constexpr uint32_t ShadowmapResolution = 2048;
    static constexpr VkFormat ShadowmapFormat     = VK_FORMAT_D32_SFLOAT;

    VulkanContext &mCtx;

    VkDescriptorPool mStaticDescriptorPool;

    Matrices                         mLightViewProjs;
    Bounds                           mBounds;
    std::array<Frustum, NumCascades> mShadowFrustums;

    struct PCDataShadow{
        glm::mat4 LightMVP;
        VkDeviceAddress VertexBuffer;
    };

    Pipeline mOpaquePipeline;
    Pipeline mAlphaPipeline;

    bool mDebugView     = false;
    bool mFreezeFrustum = false;
    bool mFitToScene    = true;

    float mShadowDist = 10.0f;

    // Main (multi layer) shadowmap texture and corresponding sampler:
    Texture   mShadowmap;
    VkSampler mSampler;
    // Single layer views for rendering subsequent cascades:
    std::array<VkImageView, NumCascades> mCascadeViews;

    VkDescriptorSetLayout mShadowmapDescriptorSetLayout;
    VkDescriptorSet       mShadowmapDescriptorSet;

    // Descriptor set for sending shadow map view to imgui:
    VkSampler       mDebugSampler;
    VkDescriptorSet mDebugTextureDescriptorSet;

    // Additional pipeline and resources for debug visualization:
    Pipeline mDebugPipeline;

    VkFormat mDebugColorFormat;
    VkFormat mDebugDepthFormat;

    GeometryLayout mDebugGeometryLayout{
        .VertexLayout = Vertex::PushLayout{},
        .IndexType    = VK_INDEX_TYPE_UINT16,
    };

    static constexpr size_t NumVertsPerFrustum = 8;
    static constexpr size_t NumIdxPerFrustum   = 36;

    std::array<glm::vec4, 2 * NumVertsPerFrustum> mVertexBufferData;

    Buffer mDebugFrustumVertexBuffer;
    Buffer mDebugFrustumIndexBuffer;

    struct PCDataDebug{
        glm::mat4 ViewProj;
        glm::vec4 Color;
    };

    DeletionQueue mMainDeletionQueue;
    DeletionQueue mPipelineDeletionQueue;
};