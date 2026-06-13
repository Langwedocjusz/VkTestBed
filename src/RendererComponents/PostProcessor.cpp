#include "PostProcessor.h"
#include "Pch.h"

#include "Barrier.h"
#include "Descriptor.h"
#include "MakeImage.h"
#include "Sampler.h"

#include "imgui.h"

PostProcessor::PostProcessor(VulkanContext &ctx)
    : mCtx(ctx), mPipelineDeletionQueue(ctx), mSwapchainDeletionQueue(ctx),
      mMainDeletionQueue(ctx)
{
    // Setup sampler state (linear, clamp to edge):
    mBloomSampler = SamplerBuilder("PostfxBloomSampler")
                        .SetMagFilter(VK_FILTER_LINEAR)
                        .SetMinFilter(VK_FILTER_LINEAR)
                        .SetAddressMode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
                        .SetMipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR)
                        .SetMaxLod(12.0f)
                        .Build(mCtx, mMainDeletionQueue);

    // Setup layout and allocate descriptor sets:
    DescriptorBindingCounts totalCounts{};

    // TODO: When this is dynamic the descriptor sets
    // will have to be re-allocated per-swapchain:
    uint32_t dsCountBase  = mDownsampleNum + mUpsampleNum;
    uint32_t dsCountFinal = 1;

    {
        auto [layout, counts] =
            DescriptorSetLayoutBuilder("PostfxBloomDescriptorSetLayout")
                .AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT)
                .AddCombinedSampler(1, VK_SHADER_STAGE_COMPUTE_BIT)
                .Build(mCtx, mMainDeletionQueue);

        mBloomDescriptorSetLayout = layout;
        totalCounts += dsCountBase * counts;
    }

    {
        auto [layout, counts] =
            DescriptorSetLayoutBuilder("PostfxFinalCompositionDescriptorSetLayout")
                .AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT)
                .AddCombinedSampler(1, VK_SHADER_STAGE_COMPUTE_BIT)
                .AddCombinedSampler(2, VK_SHADER_STAGE_COMPUTE_BIT)
                .Build(mCtx, mMainDeletionQueue);

        mFinalDescriptorSetLayout = layout;
        totalCounts += dsCountFinal * counts;
    }

    auto rawCounts          = totalCounts.ToRaw();
    auto descriptorSetCount = dsCountBase + dsCountFinal;

    mDescriptorPool =
        Descriptor::InitPool(mCtx, descriptorSetCount, rawCounts, mMainDeletionQueue);

    mBloomDownscaleDescriptorSets.resize(mDownsampleNum);
    mBloomUpscaleDescriptorSets.resize(mUpsampleNum);

    for (auto &set : mBloomDownscaleDescriptorSets)
    {
        set = Descriptor::Allocate(mCtx, mDescriptorPool, mBloomDescriptorSetLayout);
    }

    for (auto &set : mBloomUpscaleDescriptorSets)
    {
        set = Descriptor::Allocate(mCtx, mDescriptorPool, mBloomDescriptorSetLayout);
    }

    mFinalDescriptorSet =
        Descriptor::Allocate(mCtx, mDescriptorPool, mFinalDescriptorSetLayout);
}

void PostProcessor::OnImGui()
{
    ImGui::SliderFloat("Bloom Strength", &mBloomStrength, 0.0f, 1.0f);
}

void PostProcessor::RebuildPipelines()
{
    mBloomDownscalePipeline =
        ComputePipelineBuilder("PostfxBloomDownsample")
            .SetShaderPath("assets/spirv/postfx/BloomDownsampleComp.spv")
            .AddDescriptorSetLayout(mBloomDescriptorSetLayout)
            .SetPushConstantSize(sizeof(PCDataDownsample))
            .Build(mCtx, mPipelineDeletionQueue);

    mBloomUpscalePipeline =
        ComputePipelineBuilder("PostfxBloomUpsample")
            .SetShaderPath("assets/spirv/postfx/BloomUpsampleComp.spv")
            .AddDescriptorSetLayout(mBloomDescriptorSetLayout)
            .SetPushConstantSize(sizeof(PCDataUpsample))
            .Build(mCtx, mPipelineDeletionQueue);

    mFinalPipeline = ComputePipelineBuilder("PostxFinal")
                         .SetShaderPath("assets/spirv/postfx/FinalComp.spv")
                         .AddDescriptorSetLayout(mFinalDescriptorSetLayout)
                         .SetPushConstantSize(sizeof(PCDataFinal))
                         .Build(mCtx, mPipelineDeletionQueue);
}

void PostProcessor::RecreateSwapchainResources(Image      &renderTarget,
                                               VkImageView renderTargetView)
{
    // TODO: Make barriers non-coarse
    mSwapchainDeletionQueue.flush();

    // Create bloom render target:
    {
        VkExtent2D extent{
            .width  = renderTarget.Info.extent.width,
            .height = renderTarget.Info.extent.height,
        };

        Image2DInfo info{
            .Extent    = extent,
            .Format    = renderTarget.Info.format,
            .Usage     = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .MipLevels = mBloomNumMips,
            .Layout    = VK_IMAGE_LAYOUT_GENERAL,
        };

        mBloomTarget =
            MakeTexture::Texture2D(mCtx, "BloomTarget", info, mSwapchainDeletionQueue);
    }

    // Create single mip-level views for the bloom target:
    {
        mBloomSingleMipViews.resize(mBloomNumMips);

        for (uint32_t mip = 0; mip < mBloomNumMips; mip++)
        {
            auto viewName = std::string("BloomSingleMipView") + std::to_string(mip);

            auto view = MakeView::View2D(mCtx, viewName.c_str(),
                                         {.Img = mBloomTarget.Img, .SelectLevel = mip});

            mBloomSingleMipViews[mip] = view;
            mSwapchainDeletionQueue.push_back(view);
        }
    }

    // Create final (composed) target:
    {
        VkExtent2D extent{
            .width  = renderTarget.Info.extent.width,
            .height = renderTarget.Info.extent.height,
        };

        Image2DInfo info{
            .Extent    = extent,
            .Format    = FinalFormat,
            .Usage     = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            .MipLevels = 1,
            .Layout    = VK_IMAGE_LAYOUT_GENERAL,
        };

        mFinalTarget =
            MakeTexture::Texture2D(mCtx, "FinalTarget", info, mSwapchainDeletionQueue);
    }

    // Update the descriptor sets:

    // First downscale descriptor: renderTarget -> bloomTarget[0]
    DescriptorUpdater(mBloomDownscaleDescriptorSets[0])
        .WriteStorageImage(0, mBloomSingleMipViews[0])
        .WriteCombinedSampler(1, renderTargetView, mBloomSampler)
        .Update(mCtx);

    // N-1 downscale descriptors: bloomTarget[i-1] -> bloomTarget[i]
    for (size_t i = 1; i < mBloomNumMips; i++)
    {
        mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
            // Transition read image (i-1) to read only:
            auto currentRange         = Image::GetDefaultRange(mBloomTarget.Img);
            currentRange.baseMipLevel = i - 1;
            currentRange.levelCount   = 1;

            auto barrierInfo = barrier::LayoutTransitionInfo{
                .Image            = mBloomTarget.Img,
                .OldLayout        = VK_IMAGE_LAYOUT_GENERAL,
                .NewLayout        = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .SubresourceRange = currentRange,
            };

            barrier::ImageLayoutCoarse(cmd, barrierInfo);
        });

        DescriptorUpdater(mBloomDownscaleDescriptorSets[i])
            .WriteStorageImage(0, mBloomSingleMipViews[i])
            .WriteCombinedSampler(1, mBloomSingleMipViews[i - 1], mBloomSampler)
            .Update(mCtx);
    }

    // N upscale descriptors: bloomTarget[i+1] -> bloomTarget[i]

    // Transition final mip to sampled:
    mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
        // Transition read image (i) to read only:
        auto currentRange         = Image::GetDefaultRange(mBloomTarget.Img);
        currentRange.baseMipLevel = mBloomNumMips - 1;
        currentRange.levelCount   = 1;

        auto barrierInfo = barrier::LayoutTransitionInfo{
            .Image            = mBloomTarget.Img,
            .OldLayout        = VK_IMAGE_LAYOUT_GENERAL,
            .NewLayout        = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .SubresourceRange = currentRange,
        };

        barrier::ImageLayoutCoarse(cmd, barrierInfo);
    });

    for (size_t i = mBloomNumMips - 2; i != size_t(-1); i--)
    {
        mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
            // Transition target image (i) to general:
            auto rangeDst         = Image::GetDefaultRange(mBloomTarget.Img);
            rangeDst.baseMipLevel = i;
            rangeDst.levelCount   = 1;

            auto barrierDst = barrier::LayoutTransitionInfo{
                .Image            = mBloomTarget.Img,
                .OldLayout        = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .NewLayout        = VK_IMAGE_LAYOUT_GENERAL,
                .SubresourceRange = rangeDst,
            };

            barrier::ImageLayoutCoarse(cmd, barrierDst);
        });

        DescriptorUpdater(mBloomUpscaleDescriptorSets[i])
            .WriteStorageImage(0, mBloomSingleMipViews[i])
            .WriteCombinedSampler(1, mBloomSingleMipViews[i + 1], mBloomSampler)
            .Update(mCtx);
    }

    // Final composition descriptor:

    // Transition zeroth mip to sample:

    mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
        auto rangeSrc         = Image::GetDefaultRange(mBloomTarget.Img);
        rangeSrc.baseMipLevel = 0;
        rangeSrc.levelCount   = 1;

        auto barrierSrc = barrier::LayoutTransitionInfo{
            .Image            = mBloomTarget.Img,
            .OldLayout        = VK_IMAGE_LAYOUT_GENERAL,
            .NewLayout        = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .SubresourceRange = rangeSrc,
        };

        barrier::ImageLayoutCoarse(cmd, barrierSrc);
    });

    DescriptorUpdater(mFinalDescriptorSet)
        .WriteStorageImage(0, mFinalTarget.View)
        .WriteCombinedSampler(1, renderTargetView, mBloomSampler)
        .WriteCombinedSampler(2, mBloomSingleMipViews[0], mBloomSampler)
        .Update(mCtx);

    // Transition images to correct layouts:
    mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
        // Only transition 1 - N-1, 0 and N already correct:
        auto bloomRange         = Image::GetDefaultRange(mBloomTarget.Img);
        bloomRange.baseMipLevel = 1;
        bloomRange.levelCount   = mBloomNumMips - 2;

        auto barrierBloom = barrier::LayoutTransitionInfo{
            .Image            = mBloomTarget.Img,
            .OldLayout        = VK_IMAGE_LAYOUT_GENERAL,
            .NewLayout        = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .SubresourceRange = bloomRange,
        };
        barrier::ImageLayoutCoarse(cmd, barrierBloom);

        auto barrierFinal = barrier::LayoutTransitionInfo{
            .Image     = mFinalTarget.Img,
            .OldLayout = VK_IMAGE_LAYOUT_GENERAL,
            .NewLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        };
        barrier::ImageLayoutCoarse(cmd, barrierFinal);
    });
}

void PostProcessor::RunPostProcessPass(VkCommandBuffer cmd)
{
    std::vector<glm::uvec2> resolutions;
    {
        resolutions.resize(mBloomNumMips);

        uint32_t resX = mBloomTarget.Img.Info.extent.width;
        uint32_t resY = mBloomTarget.Img.Info.extent.height;

        for (size_t mip = 0; mip < mBloomNumMips; mip++)
        {
            resolutions[mip] = glm::uvec2(resX, resY);
        }

        // Update next mip resolution:
        resX = std::max(1u, resX / 2);
        resY = std::max(1u, resY / 2);
    }

    // Downsampling passes:
    for (size_t mip = 0; mip < mBloomNumMips; mip++)
    {
        auto dstRange         = Image::GetDefaultRange(mBloomTarget.Img);
        dstRange.baseMipLevel = mip;
        dstRange.levelCount   = 1;

        barrier::TextureCompToGeneral(cmd, mBloomTarget.Img, dstRange);

        if (mip > 0)
        {
            auto srcRange         = Image::GetDefaultRange(mBloomTarget.Img);
            srcRange.baseMipLevel = mip - 1;
            srcRange.levelCount   = 1;

            barrier::TextureCompToSample(cmd, mBloomTarget.Img, srcRange);
        }

        auto ds = mBloomDownscaleDescriptorSets[mip];

        mBloomDownscalePipeline.Bind(cmd);
        mBloomDownscalePipeline.BindDescriptorSet(cmd, ds, 0);

        glm::uvec2 srcResolution;

        if (mip > 0)
            srcResolution = resolutions[mip - 1];
        else
            srcResolution = resolutions[0];

        PCDataDownsample pcData{
            .SourceResolution = srcResolution,
            .RawCopy          = (mip == 0),
        };
        mBloomDownscalePipeline.PushConstants(cmd, pcData);

        auto     res        = resolutions[mip];
        uint32_t localSizeX = 16, localSizeY = 16;

        uint32_t dispCountX = (res.x + localSizeX - 1) / localSizeX;
        uint32_t dispCountY = (res.y + localSizeY - 1) / localSizeY;

        vkCmdDispatch(cmd, dispCountX, dispCountY, 1);
    }

    // Upsample passes:
    for (size_t mip = mBloomNumMips - 2; mip != size_t(-1); mip--)
    {
        auto srcRange         = Image::GetDefaultRange(mBloomTarget.Img);
        srcRange.baseMipLevel = mip + 1;
        srcRange.levelCount   = 1;

        barrier::TextureCompToSample(cmd, mBloomTarget.Img, srcRange);

        auto dstRange         = Image::GetDefaultRange(mBloomTarget.Img);
        dstRange.baseMipLevel = mip;
        dstRange.levelCount   = 1;
        
        barrier::TextureCompToGeneralRetained(cmd, mBloomTarget.Img, dstRange);

        auto ds = mBloomUpscaleDescriptorSets[mip];

        mBloomUpscalePipeline.Bind(cmd);
        mBloomUpscalePipeline.BindDescriptorSet(cmd, ds, 0);

        PCDataUpsample pcData{
            .UVRadius = 0.01,
        };
        mBloomUpscalePipeline.PushConstants(cmd, pcData);

        auto     res        = resolutions[mip];
        uint32_t localSizeX = 16, localSizeY = 16;

        uint32_t dispCountX = (res.x + localSizeX - 1) / localSizeX;
        uint32_t dispCountY = (res.y + localSizeY - 1) / localSizeY;

        vkCmdDispatch(cmd, dispCountX, dispCountY, 1);
    }

    // Final composition pass:
    {
        auto srcRange         = Image::GetDefaultRange(mBloomTarget.Img);
        srcRange.baseMipLevel = 0;
        srcRange.levelCount   = 1;

        barrier::TextureCompToSample(cmd, mBloomTarget.Img, srcRange);

        barrier::TransferSrcToGeneral(cmd, mFinalTarget.Img.Handle);

        mFinalPipeline.Bind(cmd);
        mFinalPipeline.BindDescriptorSet(cmd, mFinalDescriptorSet, 0);

        PCDataFinal pcData{
            .BloomStrength = mBloomStrength,
        };
        mFinalPipeline.PushConstants(cmd, pcData);

        auto     res        = resolutions[0];
        uint32_t localSizeX = 16, localSizeY = 16;

        uint32_t dispCountX = (res.x + localSizeX - 1) / localSizeX;
        uint32_t dispCountY = (res.y + localSizeY - 1) / localSizeY;

        vkCmdDispatch(cmd, dispCountX, dispCountY, 1);
    }

    barrier::GeneralToTransferSrc(cmd, mFinalTarget.Img.Handle);
}