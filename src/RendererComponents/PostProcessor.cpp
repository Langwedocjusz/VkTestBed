#include "PostProcessor.h"
#include "Pch.h"

#include "Barrier.h"
#include "Descriptor.h"
#include "Sampler.h"
#include "VkEnums.h"
#include "VkUtils.h"

#include "imgui.h"

PostProcessor::PostProcessor(VulkanContext &ctx, VkDescriptorPool staticPool)
    : mCtx(ctx)
    , mBloomDownscalePipeline(ctx)
    , mBloomUpscalePipeline(ctx)
    , mFinalPipeline(ctx)
    , mSwapchainDeletionQueue(ctx)
    , mMainDeletionQueue(ctx)
{
    // Setup sampler state (linear, clamp to edge):
    // clang-format off
    mBloomSampler = 
        SamplerBuilder("PostfxBloomSampler")
            .SetMagFilter(VK_FILTER_LINEAR)
            .SetMinFilter(VK_FILTER_LINEAR)
            .SetAddressMode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
            .SetMipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR)
            .SetMaxLod(12.0f)
            .Build(mCtx, mMainDeletionQueue);
    // clang-format on

    // Build the pipelines:
    RebuildPipelines();

    // Allocate descriptor sets:
    mBloomDownscalePipeline.AllocateReflectedSets(ctx, staticPool);
    mBloomUpscalePipeline.AllocateReflectedSets(ctx, staticPool);
    mFinalPipeline.AllocateReflectedSets(ctx, staticPool);
}

void PostProcessor::OnImGui()
{
    ImGui::Checkbox("Bloom Enabled", &mBloomEnabled);
    ImGui::SliderFloat("Bloom Strength", &mBloomStrength, 0.0f, 1.0f);
}

void PostProcessor::RebuildPipelines()
{
    ComputePipelineBuilder(mBloomDownscalePipeline)
        .SetDebugName("PostfxBloomDownsample")
        .SetShaderPath("assets/spirv/postfx/BloomDownsampleComp.spv")
        .Build(mCtx);

    ComputePipelineBuilder(mBloomUpscalePipeline)
        .SetDebugName("PostfxBloomUpsample")
        .SetShaderPath("assets/spirv/postfx/BloomUpsampleComp.spv")
        .Build(mCtx);

    ComputePipelineBuilder(mFinalPipeline)
        .SetDebugName("PostxFinal")
        .SetShaderPath("assets/spirv/postfx/FinalComp.spv")
        .Build(mCtx);
}

void PostProcessor::RecreateSwapchainResources(Image &renderTarget, VkImageView renderTargetView)
{
    mSwapchainDeletionQueue.flush();

    // Create bloom and final render targets:
    {
        using namespace vkenum::image_usage;

        const VkExtent2D targetExtent{
            .width  = renderTarget.Info.extent.width,
            .height = renderTarget.Info.extent.height,
        };

        // clang-format off
        mBloomTarget = Texture::Make2D(mCtx, {
            .Name      = "BloomTarget",
            .Extent    = targetExtent,
            .Format    = renderTarget.Info.format,
            .Usage     = Storage | Sampled,
            .MipLevels = mBloomNumMips,
            .Layout    = VK_IMAGE_LAYOUT_GENERAL,
        });
        
        mFinalTarget = Texture::Make2D(mCtx, {
            .Name      = "FinalTarget",
            .Extent    = targetExtent,
            .Format    = FinalFormat,
            .Usage     = Storage | TransferSrc,
            .MipLevels = 1,
            .Layout    = VK_IMAGE_LAYOUT_GENERAL,
        });
        // clang-format on
    }

    // Create single mip-level views for the bloom target:
    mBloomSingleMipViews = mBloomTarget->Img.MakeViewsSingleMip();

    for (auto view : mBloomSingleMipViews)
    {
        mSwapchainDeletionQueue.push_back(view);
    }

    // Update the descriptor sets:
    std::vector<VkSampler> mipSamplers(mBloomNumMips, mBloomSampler);

    DescriptorUpdater(mBloomDownscalePipeline.DescriptorSets.at(0))
        .WriteStorageImages(0, mBloomSingleMipViews)
        .WriteCombinedSamplers(1, mBloomSingleMipViews, mipSamplers, VK_IMAGE_LAYOUT_GENERAL)
        .WriteCombinedSampler(2, renderTargetView, mBloomSampler)
        .Update(mCtx);

    DescriptorUpdater(mBloomUpscalePipeline.DescriptorSets.at(0))
        .WriteStorageImages(0, mBloomSingleMipViews)
        .WriteCombinedSamplers(1, mBloomSingleMipViews, mipSamplers, VK_IMAGE_LAYOUT_GENERAL)
        .WriteCombinedSampler(2, renderTargetView, mBloomSampler)
        .Update(mCtx);

    mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
        using namespace vkenum::image_layout;

        barrier::ImageLayoutCoarse(cmd, {.Img = *mBloomTarget, .Old = General, .New = ShaderRead});
    });

    DescriptorUpdater(mFinalPipeline.DescriptorSets.at(0))
        .WriteStorageImage(0, mFinalTarget->View)
        .WriteCombinedSampler(1, renderTargetView, mBloomSampler)
        .WriteCombinedSampler(2, mBloomSingleMipViews[0], mBloomSampler)
        .Update(mCtx);

    // Transition final target to correct layout:
    mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
        using namespace vkenum::image_layout;

        barrier::ImageLayoutCoarse(cmd, {.Img = *mFinalTarget, .Old = General, .New = TransferSrc});
    });
}

void PostProcessor::RunPostProcessPass(VkCommandBuffer cmd)
{
    std::vector<glm::uvec2> resolutions;
    {
        resolutions.resize(mBloomNumMips);

        uint32_t resX = mBloomTarget->Img.Info.extent.width;
        uint32_t resY = mBloomTarget->Img.Info.extent.height;

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
        barrier::TextureCompToGeneral(cmd, mBloomTarget->Img);

        // Downsampling passes:
        mBloomDownscalePipeline.Bind(cmd);
        mBloomDownscalePipeline.BindReflectedSets(cmd);

        for (uint32_t mip = 0; mip < mBloomNumMips; mip++)
        {
            if (mip > 0)
            {
                auto srcRange         = mBloomTarget->Img.GetDefaultRange();
                srcRange.baseMipLevel = mip - 1;
                srcRange.levelCount   = 1;

                barrier::TextureGeneralToGeneral(cmd, mBloomTarget->Img, srcRange);
            }

            struct PCData {
                uint32_t CurrentMip;
            };

            // clang-format off
            mBloomDownscalePipeline.PushConstants(cmd, PCData{
                .CurrentMip = static_cast<uint32_t>(mip),
            });
            // clang-format on

            auto res = resolutions[mip];
            mBloomDownscalePipeline.Dispatch(cmd, res.x, res.y);
        }

        // Upsample passes:
        mBloomUpscalePipeline.Bind(cmd);
        mBloomUpscalePipeline.BindReflectedSets(cmd);

        for (uint32_t mip = mBloomNumMips - 2; mip != uint32_t(-1); mip--)
        {
            auto srcRange         = mBloomTarget->Img.GetDefaultRange();
            srcRange.baseMipLevel = mip + 1;
            srcRange.levelCount   = 1;

            barrier::TextureGeneralToGeneral(cmd, mBloomTarget->Img, srcRange);

            auto dstRange         = mBloomTarget->Img.GetDefaultRange();
            dstRange.baseMipLevel = mip;
            dstRange.levelCount   = 1;

            struct PCData {
                uint32_t CurrentMip;
            };

            // clang-format off
            mBloomUpscalePipeline.PushConstants(cmd, PCData{
                .CurrentMip = static_cast<uint32_t>(mip),
            });
            // clang-format on

            auto res = resolutions[mip];
            mBloomUpscalePipeline.Dispatch(cmd, res.x, res.y);
        }

        barrier::TextureCompToSample(cmd, mBloomTarget->Img);
    }

    // Final composition pass:
    {
        barrier::TransferSrcToGeneral(cmd, mFinalTarget->Img.Handle);

        mFinalPipeline.Bind(cmd);
        mFinalPipeline.BindReflectedSets(cmd);

        struct PCData {
            int32_t BloomEnabled;
            float   BloomStrength;
            int32_t AdditionalColorCorrect;
        };

        // clang-format off
        mFinalPipeline.PushConstants(cmd, PCData{
            .BloomEnabled           = static_cast<int32_t>(mBloomEnabled),
            .BloomStrength          = mBloomStrength,
            .AdditionalColorCorrect = !vkutils::IsFormatSRGB(mCtx.Swapchain->Format),
        });
        // clang-format on

        auto res = resolutions[0];
        mFinalPipeline.Dispatch(cmd, res.x, res.y);
    }

    barrier::GeneralToTransferSrc(cmd, mFinalTarget->Img.Handle);
}