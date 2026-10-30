#pragma once

#include "Camera.h"
#include "DeletionQueue.h"
#include "Image.h"
#include "Pipeline.h"
#include "VulkanContext.h"

#include "volk.h"

#include <memory>

class AOHandler {
  public:
    AOHandler(VulkanContext &ctx, Camera &cam, VkDescriptorPool staticPool);

    void OnImGui();

    void RebuildPipelines();

    // (Re)Creates all render targets / textures used by this module.
    // This is linked to the swapchain since their resolution
    // is scaled along with the resolution of the swapchain.
    void RecreateSwapchainResources(Image &depthBuffer, VkImageView depthOnlyView, VkExtent2D drawExtent);

    // This overload re-uses cached versions of the arguments:
    void RecreateSwapchainResources();

    // Both RebuildPipelines and RecreateSwapchainResources
    // must be called before first invocation of RunAOPass.
    // Depth buffer is assumed to be in VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL.
    void RunAOPass(VkCommandBuffer cmd);

    // Retrieve data needed to access the ao texture:
    // RecreateSwapchainResources must have been called beforehand.
    [[nodiscard]] std::pair<VkImageView, VkSampler> GetViewAndSampler() const
    {
        return {mBilateralTarget->View, mSampler};
    }

    // Whether or not the AOHandler internally requested
    // its swapchain resources to be recreated.
    [[nodiscard]] bool RecreateRequested() const
    {
        return mRecreateRequested;
    }

  private:
    struct ResourceCache {
        Image      &DepthBuffer;
        VkImageView DepthOnlyView;
        VkExtent2D  DrawExtent;
    };

    VulkanContext &mCtx;
    Camera        &mCamera;

    std::unique_ptr<ResourceCache> mResourceCache = nullptr;

    float mResolutionScale   = 0.67f;
    bool  mRecreateRequested = false;

    ComputePipeline mZGenPipeline;
    ComputePipeline mZMipGenPipeline;
    ComputePipeline mAOGenPipeline;
    ComputePipeline mBilateralPipeline;

    std::optional<Texture> mZBuffer;
    std::optional<Texture> mAOTarget;
    std::optional<Texture> mBilateralTarget;

    // TODO: Test different counts impact on perf:
    static constexpr uint32_t ZBufferMips = 4;
    std::vector<VkImageView>  mZSingleLevelViews;

    VkSampler mSampler;

    DeletionQueue mMainDeletionQueue;
    DeletionQueue mSwapchainDeletionQueue;
};