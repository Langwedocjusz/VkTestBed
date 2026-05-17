#pragma once

#include "Camera.h"
#include "DeletionQueue.h"
#include "Pipeline.h"
#include "VulkanContext.h"

#include "volk.h"

#include <memory>

class AOHandler {
  public:
    AOHandler(VulkanContext &ctx, Camera &cam);

    void OnImGui();

    void RebuildPipelines();

    // (Re)Creates all render targets / textures used by this module.
    // This is linked to the swapchain since their resolution
    // is scaled along with the resolution of the swapchain.
    void RecreateSwapchainResources(Image &depthBuffer, VkImageView depthOnlyView,
                                    VkExtent2D drawExtent);

    // This overload re-uses cached versions of the arguments:
    void RecreateSwapchainResources();

    // Both RebuildPipelines and RecreateSwapchainResources
    // must be called before first invocation of RunAOPass.
    void RunAOPass(VkCommandBuffer cmd);

    // Retrieve data needed to access the ao texture:
    // RecreateSwapchainResources must have been called beforehand.
    [[nodiscard]] std::pair<VkImageView, VkSampler> GetViewAndSampler() const
    {
        return {mAOTarget.View, mAOutSampler};
    }

    // Whether or not the AOHandler internally requested
    // its swapchain resources to be recreated.
    [[nodiscard]] bool RecreateRequested() const
    {
        return mRecreateRequested;
    }

  private:
    struct PCDataZ {
        glm::mat4 Proj;
    };

    struct PCDataAO {
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

    float mResolutionScale   = 1.0f;
    bool  mRecreateRequested = false;

    Pipeline mZGenPipeline;
    Pipeline mZMipGenPipeline;
    Pipeline mAOGenPipeline;

    Texture mZBuffer;
    Texture mAOTarget;

    // TODO: Test different counts impact on perf:
    static constexpr uint32_t            ZBufferMips = 4;
    std::array<VkImageView, ZBufferMips> mZSingleLevelViews;

    // Descriptor set for AO generation:
    VkDescriptorPool mAODescriptorPool;

    VkDescriptorSetLayout mAODescriptorSetLayout;
    VkDescriptorSet       mAOGenDescriptorSet;
    VkDescriptorSet       mZGenDescriptorSet;

    VkDescriptorSetLayout                        mZMipGenDescriptorSetLayout;
    std::array<VkDescriptorSet, ZBufferMips - 1> mZMipGenDescriptorSets;

    VkSampler mDepthSampler;
    VkSampler mAOutSampler;

    DeletionQueue mMainDeletionQueue;
    DeletionQueue mPipelineDeletionQueue;
    DeletionQueue mSwapchainDeletionQueue;
};