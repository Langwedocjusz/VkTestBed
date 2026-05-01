#include "AOHandler.h"
#include "Pch.h"

#include "Barrier.h"
#include "Descriptor.h"
#include "MakeImage.h"
#include "Sampler.h"

#include "imgui.h"

AOHandler::AOHandler(VulkanContext &ctx, Camera &camera)
    : mCtx(ctx), mCamera(camera), mMainDeletionQueue(ctx), mPipelineDeletionQueue(ctx),
      mSwapchainDeletionQueue(ctx)
{
    // For depth clamp to edge is required to avoid ao artefacts near screen edges:
    mDepthSampler = SamplerBuilder("AODepthSampler")
                        .SetMagFilter(VK_FILTER_LINEAR)
                        .SetMinFilter(VK_FILTER_LINEAR)
                        .SetAddressMode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
                        .SetMipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR)
                        .SetMaxLod(12.0f)
                        .Build(mCtx, mMainDeletionQueue);

    mAOutSampler = SamplerBuilder("AOOutSampler")
                       .SetMagFilter(VK_FILTER_LINEAR)
                       .SetMinFilter(VK_FILTER_LINEAR)
                       .SetAddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT)
                       .SetMipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR)
                       .SetMaxLod(12.0f)
                       .Build(mCtx, mMainDeletionQueue);

    // Build descriptor sets for AO:
    {
        auto [layout, counts] = DescriptorSetLayoutBuilder("AOGenDSLayout")
                               .AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT)
                               .AddCombinedSampler(1, VK_SHADER_STAGE_COMPUTE_BIT)
                               .Build(mCtx, mMainDeletionQueue);

        auto rawCounts = counts.ToRaw();                   
        mAODescriptorPool = Descriptor::InitPool(mCtx, 2, rawCounts, mMainDeletionQueue);

        mAOGenDescriptorSetLayout = layout;
        mAOGenDescriptorSet       = Descriptor::Allocate(mCtx, mAODescriptorPool, layout);
    }
}

void AOHandler::OnImGui()
{
    ImGui::SliderFloat("Resolution scale", &mResolutionScale, 0.25f, 1.0f);

    if (ImGui::Button("Recreate target"))
    {
        vkDeviceWaitIdle(mCtx.Device);

        RecreateSwapchainResources(mResourceCache->DepthBuffer,
                                   mResourceCache->DepthOnlyView,
                                   mResourceCache->DrawExtent);
    }
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
                                           VkExtent2D drawExtent)
{
    mSwapchainDeletionQueue.flush();

    // Update resource cache:
    mResourceCache = std::make_unique<ResourceCache>(ResourceCache{
        .DepthBuffer   = depthBuffer,
        .DepthOnlyView = depthOnlyView,
        .DrawExtent    = drawExtent,
    });

    // Create target for occlusion computation:
    auto ScaleResolution = [this](uint32_t res) {
        return static_cast<uint32_t>(mResolutionScale * static_cast<float>(res));
    };

    VkExtent2D targetExtent{
        .width  = ScaleResolution(drawExtent.width),
        .height = ScaleResolution(drawExtent.height),
    };

    Image2DInfo aoTargetInfo{
        .Extent = targetExtent,
        .Format = VK_FORMAT_R8G8B8A8_UNORM,
        .Usage  = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .Layout = VK_IMAGE_LAYOUT_GENERAL,
    };

    mAOTarget =
        MakeTexture::Texture2D(mCtx, "AOTarget", aoTargetInfo, mSwapchainDeletionQueue);

    // Transition depth buffer to be able to update descriptor:
    mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
        auto barrierInfo = barrier::LayoutTransitionInfo{
            .Image     = depthBuffer,
            .OldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .NewLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };

        barrier::ImageLayoutCoarse(cmd, barrierInfo);
    });

    // Update AO descriptor to point to depth buffer
    DescriptorUpdater(mAOGenDescriptorSet)
        .WriteStorageImage(0, mAOTarget.View)
        .WriteCombinedSampler(1, depthOnlyView, mDepthSampler)
        .Update(mCtx);

    // Transition depth buffer and ao target to correct layouts:
    mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
        auto barrierInfo = barrier::LayoutTransitionInfo{
            .Image     = depthBuffer,
            .OldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .NewLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        };
        barrier::ImageLayoutCoarse(cmd, barrierInfo);

        auto barrierInfoAO = barrier::LayoutTransitionInfo{
            .Image     = mAOTarget.Img,
            .OldLayout = VK_IMAGE_LAYOUT_GENERAL,
            .NewLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
        barrier::ImageLayoutCoarse(cmd, barrierInfoAO);
    });
}

void AOHandler::RunAOPass(VkCommandBuffer cmd)
{
    // TODO: make the layout barriers non-coarse

    // Transition depth target to be used as texture:
    Image& depthBuffer = mResourceCache->DepthBuffer;

    auto barrierInfoDepth = barrier::LayoutTransitionInfo{
        .Image     = depthBuffer,
        .OldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .NewLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    barrier::ImageLayoutCoarse(cmd, barrierInfoDepth);

    // Transition AO target to be used as storage image (can discard previous contents):
    auto barrierInfoAO = barrier::LayoutTransitionInfo{
        .Image     = mAOTarget.Img,
        .OldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .NewLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    barrier::ImageLayoutCoarse(cmd, barrierInfoAO);

    // Calculate ambient occlusion:
    mAOGenPipeline.Bind(cmd);
    mAOGenPipeline.BindDescriptorSet(cmd, mAOGenDescriptorSet, 0);

    auto frBack = mCamera.GetFrustumBackEye();

    PCDataAO data{
        .Proj    = mCamera.GetProj(),
        //.InvProj = glm::inverse(mCamera.GetProj()),
        .TopLeft = frBack.TopLeft,
        .TopRight = frBack.TopRight,
        .BottomLeft = frBack.BottomLeft ,
        .BottomRight = frBack.BottomRight,
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
    barrier::ImageLayoutCoarse(cmd, barrierInfoAO);

    // Transition depth target back to be used as depth attachment:
    barrierInfoDepth.OldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrierInfoDepth.NewLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    barrier::ImageLayoutCoarse(cmd, barrierInfoDepth);
}