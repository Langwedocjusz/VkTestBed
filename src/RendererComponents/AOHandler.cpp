#include "AOHandler.h"
#include "ImageUtils.h"
#include "Pch.h"

#include "Barrier.h"
#include "Descriptor.h"
#include "Sampler.h"

AOHandler::AOHandler(VulkanContext &ctx)
    : mCtx(ctx), mDeletionQueue(ctx), mPipelineDeletionQueue(ctx),
      mSwapchainDeletionQueue(ctx)
{
    // For ao clamp to edge is required to avoid artefacts near screen edges:
    mAOSampler = SamplerBuilder("AOSampler")
                     .SetMagFilter(VK_FILTER_LINEAR)
                     .SetMinFilter(VK_FILTER_LINEAR)
                     .SetAddressMode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
                     .SetMipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR)
                     .SetMaxLod(12.0f)
                     .Build(mCtx, mDeletionQueue);

    // Build descriptor sets for AO:
    {
        // clang-format off
        std::array<VkDescriptorPoolSize, 2> poolCounts{{
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2},
            {         VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
        }};
        // clang-format on

        mAODescriptorPool = Descriptor::InitPool(mCtx, 2, poolCounts);
        mDeletionQueue.push_back(mAODescriptorPool);
    }

    mAOGenDescriptorSetLayout = DescriptorSetLayoutBuilder("AOGenDSLayout")
                                    .AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT)
                                    .AddCombinedSampler(1, VK_SHADER_STAGE_COMPUTE_BIT)
                                    .Build(mCtx, mDeletionQueue);

    mAOGenDescriptorSet =
        Descriptor::Allocate(mCtx, mAODescriptorPool, mAOGenDescriptorSetLayout);

    mAOUsageDescriptorSetLayout = DescriptorSetLayoutBuilder("AOUsageDSLayout")
                                      .AddCombinedSampler(0, VK_SHADER_STAGE_FRAGMENT_BIT)
                                      .Build(mCtx, mDeletionQueue);

    mAOUsageDescriptorSet =
        Descriptor::Allocate(mCtx, mAODescriptorPool, mAOUsageDescriptorSetLayout);
}

void AOHandler::RebuildPipelines()
{
    mPipelineDeletionQueue.flush();

    mAOGenPipeline = ComputePipelineBuilder("AOGenPipeline")
                         .SetShaderPath("assets/spirv/AOGenComp.spv")
                         .AddDescriptorSetLayout(mAOGenDescriptorSetLayout)
                         .SetPushConstantSize(sizeof(PCDataAO))
                         .Build(mCtx, mPipelineDeletionQueue);
}

void AOHandler::RecreateSwapchainResources(Image &depthBuffer, VkImageView depthOnlyView,
                                           VkExtent2D drawExtent, VkSampler outputSampler)
{
    // Create target for occlusion computation:
    Image2DInfo aoTargetInfo{
        .Extent = drawExtent,
        .Format = VK_FORMAT_R8G8B8A8_UNORM,
        .Usage  = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .Layout = VK_IMAGE_LAYOUT_GENERAL,
    };
    mAOTarget =
        MakeTexture::Texture2D(mCtx, "AOTarget", aoTargetInfo, mSwapchainDeletionQueue);

    // Transition depth buffer to be able to update descriptor:
    mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
        auto barrierInfo = barrier::ImageLayoutBarrierInfo{
            .Image            = depthBuffer.Handle,
            .OldLayout        = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .NewLayout        = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .SubresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
                                 0, 1, 0, 1},
        };

        barrier::ImageLayoutBarrierCoarse(cmd, barrierInfo);
    });

    // Update AO descriptor to point to depth buffer
    DescriptorUpdater(mAOGenDescriptorSet)
        .WriteStorageImage(0, mAOTarget.View)
        .WriteCombinedSampler(1, depthOnlyView, mAOSampler)
        .Update(mCtx);

    // Transition depth buffer and ao target to correct layouts:
    mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
        auto barrierInfo = barrier::ImageLayoutBarrierInfo{
            .Image            = depthBuffer.Handle,
            .OldLayout        = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .NewLayout        = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .SubresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
                                 0, 1, 0, 1},
        };
        barrier::ImageLayoutBarrierCoarse(cmd, barrierInfo);

        auto barrierInfoAO = barrier::ImageLayoutBarrierInfo{
            .Image            = mAOTarget.Img.Handle,
            .OldLayout        = VK_IMAGE_LAYOUT_GENERAL,
            .NewLayout        = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .SubresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        };
        barrier::ImageLayoutBarrierCoarse(cmd, barrierInfoAO);
    });

    DescriptorUpdater(mAOUsageDescriptorSet)
        .WriteCombinedSampler(0, mAOTarget.View, outputSampler)
        .Update(mCtx);
}

void AOHandler::RunAOPass(VkCommandBuffer cmd, Image &depthBuffer, glm::mat4 proj)
{
    // TODO: make the layout barriers non-coarse

    // Transition depth target to be used as texture:
    auto barrierInfoDepth = barrier::ImageLayoutBarrierInfo{
        .Image            = depthBuffer.Handle,
        .OldLayout        = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .NewLayout        = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .SubresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0,
                             1, 0, 1},
    };
    barrier::ImageLayoutBarrierCoarse(cmd, barrierInfoDepth);

    // Transition AO target to be used as storage image:
    auto barrierInfoAO = barrier::ImageLayoutBarrierInfo{
        .Image            = mAOTarget.Img.Handle,
        .OldLayout        = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .NewLayout        = VK_IMAGE_LAYOUT_GENERAL,
        .SubresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
    };
    barrier::ImageLayoutBarrierCoarse(cmd, barrierInfoAO);

    // Calculate ambient occlusion:
    mAOGenPipeline.Bind(cmd);
    mAOGenPipeline.BindDescriptorSet(cmd, mAOGenDescriptorSet, 0);

    PCDataAO data{
        .Proj    = proj,
        .InvProj = glm::inverse(proj),
    };

    mAOGenPipeline.PushConstants(cmd, data);

    float localSizeX = 32.0f, localSizeY = 32.0f;

    auto targetWidth  = static_cast<float>(mAOTarget.Img.Info.extent.width);
    auto targetHeight = static_cast<float>(mAOTarget.Img.Info.extent.height);

    auto dispCountX = static_cast<uint32_t>(std::ceil(targetWidth / localSizeX));
    auto dispCountY = static_cast<uint32_t>(std::ceil(targetHeight / localSizeY));

    vkCmdDispatch(cmd, dispCountX, dispCountY, 1);

    // Transition AO target back to be used as a texture:
    barrierInfoAO.OldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrierInfoAO.NewLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier::ImageLayoutBarrierCoarse(cmd, barrierInfoAO);

    // Transition depth target back to be used as depth attachment:
    barrierInfoDepth.OldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrierInfoDepth.NewLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    barrier::ImageLayoutBarrierCoarse(cmd, barrierInfoDepth);
}