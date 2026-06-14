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
    // rgba8_srgb | transfer_src | storgate image:
    static constexpr VkFormat FinalFormat = VK_FORMAT_R8G8B8A8_UNORM;
    Texture                   mFinalTarget;

    float mBloomStrength = 0.05f;

    // TODO: This should be scaled along with screen resolution
    // to make the effect consistent:
    const uint32_t mBloomNumMips  = 5;
    const uint32_t mDownsampleNum = mBloomNumMips;
    const uint32_t mUpsampleNum   = mBloomNumMips - 1;

    Pipeline mBloomDownscalePipeline;
    Pipeline mBloomUpscalePipeline;
    Pipeline mFinalPipeline;

    struct PCDataDownsample {
        glm::uvec2 SourceResolution;
        uint32_t   RawCopy;
    };

    struct PCDataUpsample {
        glm::uvec2 SourceResolution;
    };

    struct PCDataFinal {
        float BloomStrength;
    };

    VkSampler                mBloomSampler;
    Texture                  mBloomTarget;
    std::vector<VkImageView> mBloomSingleMipViews;

    // For both downscale and upscale (src image -> dst image):
    VkDescriptorSetLayout        mBloomDescriptorSetLayout;
    std::vector<VkDescriptorSet> mBloomDownscaleDescriptorSets;
    std::vector<VkDescriptorSet> mBloomUpscaleDescriptorSets;

    // For the final composition:
    VkDescriptorSetLayout mFinalDescriptorSetLayout;
    VkDescriptorSet       mFinalDescriptorSet;

    VkDescriptorPool mDescriptorPool;

    DeletionQueue mPipelineDeletionQueue;
    DeletionQueue mSwapchainDeletionQueue;
    DeletionQueue mMainDeletionQueue;
};
