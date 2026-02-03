#pragma once

#include "Camera.h"
#include "DeletionQueue.h"
#include "GeometryData.h"
#include "Pipeline.h"
#include "VertexLayout.h"
#include "VulkanContext.h"

#include "volk.h"
#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

class ShadowmapHandler {
  public:
    ShadowmapHandler(VulkanContext &ctx, VkFormat debugColorFormat, VkFormat debugDepthFormat);
    ~ShadowmapHandler();

    void RebuildPipelines(const Vertex::Layout &vertexLayout,
                          VkDescriptorSetLayout materialDSLayout);
    void OnUpdate(Frustum camFr, glm::vec3 lightDir, AABB sceneAABB);
    void OnImGui();

    void BeginShadowPass(VkCommandBuffer cmd);
    void EndShadowPass(VkCommandBuffer cmd);

    /// Descriptor set being bound is assumed to have albedo map as its first binding.
    /// It is also assumed that transparency is stored in the a channel.
    void BindMaterialDS(VkCommandBuffer cmd, VkDescriptorSet materialDS);
    void PushConstantTransform(VkCommandBuffer cmd, glm::mat4 transform);

    [[nodiscard]] glm::mat4 GetViewProj() const
    {
        return mLightViewProj;
    }
    [[nodiscard]] VkDescriptorSetLayout GetDSLayout() const
    {
        return mShadowmapDescriptorSetLayout;
    }
    [[nodiscard]] VkDescriptorSet GetDescriptorSet() const
    {
        return mShadowmapDescriptorSet;
    }

    void DrawDebugShapes(VkCommandBuffer cmd, glm::mat4 viewProj, VkExtent2D extent);

  private:
    static constexpr uint32_t mShadowmapResolution = 2048;
    static constexpr VkFormat mShadowmapFormat = VK_FORMAT_D32_SFLOAT;

    VulkanContext &mCtx;

    VkDescriptorPool mStaticDescriptorPool;
    Pipeline mShadowmapPipeline;

    glm::mat4 mLightViewProj;
    Frustum mShadowFrustum;

    struct {
        glm::mat4 LightMVP;
    } mShadowPCData;

    bool mDebugView = false;
    bool mFreezeFrustum = false;
    bool mFitToScene = true;

    float mShadowDist = 20.0f;

    Image mShadowmap;
    VkImageView mShadowmapView;
    VkSampler mSamplerShadowmap;

    VkDescriptorSetLayout mShadowmapDescriptorSetLayout;
    VkDescriptorSet mShadowmapDescriptorSet;

    // Descriptor set for sending shadow map view to imgui:
    VkSampler mSamplerDebug;
    VkDescriptorSet mDebugTextureDescriptorSet;

    // Additional pipeline and resources for debug visualization:
    Pipeline mDebugPipeline;

    VkFormat mDebugColorFormat;
    VkFormat mDebugDepthFormat;

    GeometryLayout mDebugGeometryLayout{
        .VertexLayout = {Vertex::AttributeType::Vec4},
        .IndexType = VK_INDEX_TYPE_UINT16,
    };

    std::array<glm::vec4, 16> mVertexBufferData;

    Buffer mDebugFrustumVertexBuffer;
    Buffer mDebugFrustumIndexBuffer;

    struct {
        glm::mat4 ViewProj;
        glm::vec4 Color;
    } mDebugPCData;

    DeletionQueue mMainDeletionQueue;
    DeletionQueue mPipelineDeletionQueue;
};