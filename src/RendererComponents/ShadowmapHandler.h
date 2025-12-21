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

    [[nodiscard]] glm::mat4 GetViewProj() const
    {
        return mLightViewProj;
    }
    [[nodiscard]] VkDescriptorSetLayout GetDSLayout() const
    {
        return mShadowmapDescriptorSetLayout;
    }
    [[nodiscard]] VkDescriptorSet *GetDSPtr()
    {
        return &mShadowmapDescriptorSet;
    }

    // To-do: this shouldn't be exposed
    [[nodiscard]] VkPipelineLayout GetPipelineLayout() const
    {
        return mShadowmapPipeline.Layout;
    }

    // To-do: this is temporary, should be private:
    struct {
        glm::mat4 LightMVP;
    } mShadowPCData;

  private:
    static constexpr VkFormat mShadowmapFormat = VK_FORMAT_D32_SFLOAT;

    VulkanContext &mCtx;

    VkDescriptorPool mStaticDescriptorPool;
    Pipeline mShadowmapPipeline;

    glm::mat4 mLightViewProj;

    float mAddZ = 8.0f;
    float mSubZ = 20.0f;
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