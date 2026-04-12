#pragma once

#include "DeletionQueue.h"
#include "Pipeline.h"
#include "VulkanContext.h"

#include "volk.h"

#include <memory>

class AOHandler {
  public:
    AOHandler(VulkanContext &ctx);

    void OnImGui();

    void RebuildPipelines();
    void RecreateSwapchainResources(Image &depthBuffer, VkImageView depthOnlyView,
                                    VkExtent2D drawExtent);

    [[nodiscard]] VkDescriptorSetLayout GetDSLayout() const
    {
        return mAOUsageDescriptorSetLayout;
    }
    [[nodiscard]] VkDescriptorSet GetDescriptorSet() const
    {
        return mAOUsageDescriptorSet;
    }

    void RunAOPass(VkCommandBuffer cmd, Image &depthBuffer, glm::mat4 proj);

  private:
    struct PCDataAO {
        glm::mat4 Proj;
        glm::mat4 InvProj;
    };

    struct ResourceCache {
        Image      &DepthBuffer;
        VkImageView DepthOnlyView;
        VkExtent2D  DrawExtent;
    };

    std::unique_ptr<ResourceCache> mResourceCache = nullptr;

    float mResolutionScale = 1.0f;

    Pipeline mAOGenPipeline;

    Texture mAOTarget;

    VkDescriptorPool mAODescriptorPool;
    // Generation:
    VkDescriptorSetLayout mAOGenDescriptorSetLayout;
    VkDescriptorSet       mAOGenDescriptorSet;
    // Usage:
    VkDescriptorSetLayout mAOUsageDescriptorSetLayout;
    VkDescriptorSet       mAOUsageDescriptorSet;

    VkSampler mDepthSampler;
    VkSampler mAOutSampler;

    VulkanContext &mCtx;
    DeletionQueue  mMainDeletionQueue;
    DeletionQueue  mPipelineDeletionQueue;
    DeletionQueue  mSwapchainDeletionQueue;
};