#pragma once

#include "DeletionQueue.h"
#include "Pipeline.h"
#include "VulkanContext.h"

#include "volk.h"
#include "vulkan/vulkan_core.h"

class AOHandler {
  public:
    AOHandler(VulkanContext &ctx);
    ~AOHandler();

    void RebuildPipelines();
    void RecreateSwapchainResources(Image &depthBuffer, VkImageView depthOnlyView,
                                    VkExtent2D drawExtent, VkSampler outputSampler);

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

    Pipeline mAOGenPipeline;

    Texture mAOTarget;

    VkDescriptorPool mAODescriptorPool;
    // Generation:
    VkDescriptorSetLayout mAOGenDescriptorSetLayout;
    VkDescriptorSet       mAOGenDescriptorSet;
    // Usage:
    VkDescriptorSetLayout mAOUsageDescriptorSetLayout;
    VkDescriptorSet       mAOUsageDescriptorSet;

    VkSampler mAOSampler;

    VulkanContext &mCtx;
    DeletionQueue  mDeletionQueue;
    DeletionQueue  mPipelineDeletionQueue;
    DeletionQueue  mSwapchainDeletionQueue;
};