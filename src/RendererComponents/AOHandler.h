#pragma once

#include "Camera.h"
#include "DeletionQueue.h"
#include "Pipeline.h"
#include "VulkanContext.h"

#include "volk.h"

#include <memory>

class AOHandler {
  public:
    AOHandler(VulkanContext &ctx, Camera& cam);

    void OnImGui();

    void RebuildPipelines();

    // (Re)Creates all render targets / textures used by this module.
    // This is linked to the swapchain since their resolution
    // is scaled along with the resolution of the swapchain.
    void RecreateSwapchainResources(Image &depthBuffer, VkImageView depthOnlyView,
                                    VkExtent2D drawExtent);

    // Both RebuildPipelines and RecreateSwapchainResources 
    // must be called before first invocation of RunAOPass.
    void RunAOPass(VkCommandBuffer cmd);

    // Retrieve data needed to access the ao texture:
    // RecreateSwapchainResources must have been called beforehand.
    [[nodiscard]] std::pair<VkImageView, VkSampler> GetViewAndSampler() const
    {
        return {mAOTarget.View, mAOutSampler};
    }

  private:
    struct PCDataAO {
        glm::mat4 Proj;
        //glm::mat4 InvProj;
        glm::vec4 TopLeft;
        glm::vec4 TopRight;
        glm::vec4 BottomLeft;
        glm::vec4 BottomRight;
    };

    struct ResourceCache {
        Image      &DepthBuffer;
        VkImageView DepthOnlyView;
        VkExtent2D  DrawExtent;
    };

    VulkanContext &mCtx;
    Camera        &mCamera;

    std::unique_ptr<ResourceCache> mResourceCache = nullptr;

    float mResolutionScale = 1.0f;

    Pipeline mAOGenPipeline;

    Texture mAOTarget;

    // Descriptor set for AO generation:
    VkDescriptorPool mAODescriptorPool;
    VkDescriptorSetLayout mAOGenDescriptorSetLayout;
    VkDescriptorSet       mAOGenDescriptorSet;

    //// Usage:
    //VkDescriptorSetLayout mAOUsageDescriptorSetLayout;
    //VkDescriptorSet       mAOUsageDescriptorSet;

    VkSampler mDepthSampler;
    VkSampler mAOutSampler;

    DeletionQueue  mMainDeletionQueue;
    DeletionQueue  mPipelineDeletionQueue;
    DeletionQueue  mSwapchainDeletionQueue;
};