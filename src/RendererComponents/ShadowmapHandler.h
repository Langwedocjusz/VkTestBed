#pragma once

#include "Camera.h"
#include "DeletionQueue.h"
#include "Pipeline.h"
#include "VulkanContext.h"

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

class ShadowmapHandler {
  public:
    ShadowmapHandler(VulkanContext &ctx);
    ~ShadowmapHandler();

    void RebuildPipelines(const Vertex::Layout &vertexLayout,
                          VkDescriptorSetLayout materialDSLayout);
    void OnUpdate(Frustum camFr, glm::vec3 lightDir);
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

  private:
    static constexpr VkFormat mShadowmapFormat = VK_FORMAT_D32_SFLOAT;

    VulkanContext &mCtx;

    VkDescriptorPool mStaticDescriptorPool;
    Pipeline mShadowmapPipeline;

    glm::mat4 mLightViewProj;

    struct {
        glm::mat4 LightMVP;
    } mShadowPCData;

    float mAddZ = 8.0f;
    float mSubZ = 30.0f;
    float mShadowDist = 20.0f;

    Image mShadowmap;
    VkImageView mShadowmapView;
    VkSampler mSamplerShadowmap;

    VkDescriptorSetLayout mShadowmapDescriptorSetLayout;
    VkDescriptorSet mShadowmapDescriptorSet;

    // Descriptor set for sending shadow map view to imgui:
    VkSampler mSamplerDebug;
    VkDescriptorSet mDebugTextureDescriptorSet;

    DeletionQueue mMainDeletionQueue;
    DeletionQueue mPipelineDeletionQueue;
};