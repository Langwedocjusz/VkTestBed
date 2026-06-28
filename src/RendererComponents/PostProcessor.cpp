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
    const uint32_t dsCountBloom = 1;
    const uint32_t dsCountFinal = 1;

    {
        auto [layout, counts] =
            DescriptorSetLayoutBuilder("PostfxBloomDescriptorSetLayout")
                .AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, mBloomNumMips)
                .AddCombinedSampler(1, VK_SHADER_STAGE_COMPUTE_BIT, mBloomNumMips)
                .AddCombinedSampler(2, VK_SHADER_STAGE_COMPUTE_BIT)
                .Build(mCtx, mMainDeletionQueue);

        mBloomDescriptorSetLayout = layout;
        totalCounts += dsCountBloom * counts;
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
    auto descriptorSetCount = dsCountBloom + dsCountFinal;

    mDescriptorPool =
        Descriptor::InitPool(mCtx, descriptorSetCount, rawCounts, mMainDeletionQueue);

    mBloomDescriptorSet =
        Descriptor::Allocate(mCtx, mDescriptorPool, mBloomDescriptorSetLayout);

    mFinalDescriptorSet =
        Descriptor::Allocate(mCtx, mDescriptorPool, mFinalDescriptorSetLayout);
}

void PostProcessor::OnImGui()
{
    ImGui::Checkbox("Bloom Enabled", &mBloomEnabled);
    ImGui::SliderFloat("Bloom Strength", &mBloomStrength, 0.0f, 1.0f);
}

void PostProcessor::RebuildPipelines()
{
    mBloomDownscalePipeline =
        ComputePipelineBuilder("PostfxBloomDownsample")
            .SetShaderPath("assets/spirv/postfx/BloomDownsampleComp.spv")
            .AddDescriptorSetLayout(mBloomDescriptorSetLayout)
            .SetPushConstantSize(sizeof(PCDataBloom))
            .Build(mCtx, mPipelineDeletionQueue);

    mBloomUpscalePipeline =
        ComputePipelineBuilder("PostfxBloomUpsample")
            .SetShaderPath("assets/spirv/postfx/BloomUpsampleComp.spv")
            .AddDescriptorSetLayout(mBloomDescriptorSetLayout)
            .SetPushConstantSize(sizeof(PCDataBloom))
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

    std::vector<VkSampler> mipSamplers(mBloomNumMips, mBloomSampler);

    DescriptorUpdater(mBloomDescriptorSet)
        .WriteStorageImages(0, mBloomSingleMipViews)
        .WriteCombinedSamplers(1, mBloomSingleMipViews, mipSamplers,
                               VK_IMAGE_LAYOUT_GENERAL)
        .WriteCombinedSampler(2, renderTargetView, mBloomSampler)
        .Update(mCtx);

    mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
        auto barrierFinal = barrier::LayoutTransitionInfo{
            .Image     = mBloomTarget.Img,
            .OldLayout = VK_IMAGE_LAYOUT_GENERAL,
            .NewLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
        barrier::ImageLayoutCoarse(cmd, barrierFinal);
    });

    DescriptorUpdater(mFinalDescriptorSet)
        .WriteStorageImage(0, mFinalTarget.View)
        .WriteCombinedSampler(1, renderTargetView, mBloomSampler)
        .WriteCombinedSampler(2, mBloomSingleMipViews[0], mBloomSampler)
        .Update(mCtx);

    // Transition final target to correct layout:
    mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
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

            // Update next mip resolution:
            resX = std::max(1u, resX / 2);
            resY = std::max(1u, resY / 2);
        }
    }

    if (mBloomEnabled)
    {
        barrier::TextureCompToGeneral(cmd, mBloomTarget.Img);

        // Downsampling passes:
        mBloomDownscalePipeline.Bind(cmd);
        mBloomDownscalePipeline.BindDescriptorSet(cmd, mBloomDescriptorSet, 0);

        for (size_t mip = 0; mip < mBloomNumMips; mip++)
        {
            if (mip > 0)
            {
                auto srcRange         = Image::GetDefaultRange(mBloomTarget.Img);
                srcRange.baseMipLevel = mip - 1;
                srcRange.levelCount   = 1;

                barrier::TextureGeneralToGeneral(cmd, mBloomTarget.Img, srcRange);
            }

            PCDataBloom pcData{
                .CurrentMip = static_cast<uint32_t>(mip),
            };

            mBloomDownscalePipeline.PushConstants(cmd, pcData);

            auto     res        = resolutions[mip];
            uint32_t localSizeX = 16, localSizeY = 16;

            uint32_t dispCountX = (res.x + localSizeX - 1) / localSizeX;
            uint32_t dispCountY = (res.y + localSizeY - 1) / localSizeY;

            vkCmdDispatch(cmd, dispCountX, dispCountY, 1);
        }

        // Upsample passes:
        mBloomUpscalePipeline.Bind(cmd);
        mBloomUpscalePipeline.BindDescriptorSet(cmd, mBloomDescriptorSet, 0);

        for (size_t mip = mBloomNumMips - 2; mip != size_t(-1); mip--)
        {
            auto srcRange         = Image::GetDefaultRange(mBloomTarget.Img);
            srcRange.baseMipLevel = mip + 1;
            srcRange.levelCount   = 1;

            barrier::TextureGeneralToGeneral(cmd, mBloomTarget.Img, srcRange);

            auto dstRange         = Image::GetDefaultRange(mBloomTarget.Img);
            dstRange.baseMipLevel = mip;
            dstRange.levelCount   = 1;

            PCDataBloom pcData{
                .CurrentMip = static_cast<uint32_t>(mip),
            };

            mBloomUpscalePipeline.PushConstants(cmd, pcData);

            auto     res        = resolutions[mip];
            uint32_t localSizeX = 16, localSizeY = 16;

            uint32_t dispCountX = (res.x + localSizeX - 1) / localSizeX;
            uint32_t dispCountY = (res.y + localSizeY - 1) / localSizeY;

            vkCmdDispatch(cmd, dispCountX, dispCountY, 1);
        }

        barrier::TextureCompToSample(cmd, mBloomTarget.Img);
    }

    // Final composition pass:
    {
        barrier::TransferSrcToGeneral(cmd, mFinalTarget.Img.Handle);

        mFinalPipeline.Bind(cmd);
        mFinalPipeline.BindDescriptorSet(cmd, mFinalDescriptorSet, 0);

        PCDataFinal pcData{
            .BloomEnabled  = static_cast<int32_t>(mBloomEnabled),
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