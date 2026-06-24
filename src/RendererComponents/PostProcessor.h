#pragma once

#include "Pipeline.h"
#include "Texture.h"

class PostProcessor {
  public:
    PostProcessor(VulkanContext &ctx);

    void OnImGui();

    void RebuildPipelines();

    // Render target should be in SHADER_READ_ONLY_OPTIMAL layout before calling this:
    void RecreateSwapchainResources(Image &renderTarget, VkImageView renderTargetView);

    // Render target should be in SHADER_READ_ONLY_OPTIMAL layout before calling this:
    void RunPostProcessPass(VkCommandBuffer cmd);

    Image &GetTargetImage()
    {
        return mFinalTarget.Img;
    }

  private:
    VulkanContext &mCtx;

    // Using UNORM, since my driver didn't support image with
    // rgba8_srgb | transfer_src | storage image:
    static constexpr VkFormat FinalFormat = VK_FORMAT_R8G8B8A8_UNORM;
    Texture                   mFinalTarget;

    bool  mBloomEnabled  = true;
    float mBloomStrength = 0.1f;

    // TODO: This should be scaled along with screen resolution
    // to make the effect consistent:
    const uint32_t mBloomNumMips = 5;

    Pipeline mBloomDownscalePipeline;
    Pipeline mBloomUpscalePipeline;
    Pipeline mFinalPipeline;

    struct PCDataBloom {
        uint32_t CurrentMip;
    };

    struct PCDataFinal {
        int   BloomEnabled;
        float BloomStrength;
    };

    VkSampler                mBloomSampler;
    Texture                  mBloomTarget;
    std::vector<VkImageView> mBloomSingleMipViews;

    VkDescriptorSetLayout mBloomDescriptorSetLayout;
    VkDescriptorSet       mBloomDescriptorSet;

    // For the final composition:
    VkDescriptorSetLayout mFinalDescriptorSetLayout;
    VkDescriptorSet       mFinalDescriptorSet;

    VkDescriptorPool mDescriptorPool;

    DeletionQueue mPipelineDeletionQueue;
    DeletionQueue mSwapchainDeletionQueue;
    DeletionQueue mMainDeletionQueue;
};
