#include "AOHandler.h"
#include "Pch.h"

#include "Barrier.h"
#include "Descriptor.h"
#include "MakeImage.h"
#include "Pipeline.h"
#include "Sampler.h"

#include "imgui.h"

AOHandler::AOHandler(VulkanContext &ctx, Camera &camera, VkDescriptorPool staticPool)
    : mCtx(ctx), mCamera(camera), mMainDeletionQueue(ctx), mPipelineDeletionQueue(ctx),
      mSwapchainDeletionQueue(ctx)
{
    // For screen-space depth/ao clamp to edge is required to avoid ao artefacts near
    // screen edges:
    mSampler = SamplerBuilder("AODepthSampler")
                   .SetMagFilter(VK_FILTER_LINEAR)
                   .SetMinFilter(VK_FILTER_LINEAR)
                   .SetAddressMode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
                   .SetMipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR)
                   .SetMaxLod(12.0f)
                   .Build(mCtx, mMainDeletionQueue);

    // Build descriptor sets for z buffer and AO generation:
    mAODescriptorSetLayout = DescriptorSetLayoutBuilder("AOGenDSLayout")
                                 .AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT)
                                 .AddCombinedSampler(1, VK_SHADER_STAGE_COMPUTE_BIT)
                                 .Build(mCtx, mMainDeletionQueue);

    mZMipGenDescriptorSetLayout =
        DescriptorSetLayoutBuilder("AOGenZMipDSLayout")
            .AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT, ZBufferMips)
            .Build(mCtx, mMainDeletionQueue);

    mBilateralDescriptorSetLayout = DescriptorSetLayoutBuilder("AOGenDSLayout")
                                .AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT)
                                .AddCombinedSampler(1, VK_SHADER_STAGE_COMPUTE_BIT)
                                .AddCombinedSampler(2, VK_SHADER_STAGE_COMPUTE_BIT)
                                .Build(mCtx, mMainDeletionQueue);

    mAOGenDescriptorSet =
        Descriptor::Allocate(mCtx, staticPool, mAODescriptorSetLayout);

    mZGenDescriptorSet =
        Descriptor::Allocate(mCtx, staticPool, mAODescriptorSetLayout);

    mZMipGenDescriptorSet =
        Descriptor::Allocate(mCtx, staticPool, mZMipGenDescriptorSetLayout);

    mBilateralDescriptorSet =
        Descriptor::Allocate(mCtx, staticPool, mBilateralDescriptorSetLayout);
}

void AOHandler::OnImGui()
{
    ImGui::SliderFloat("Resolution scale", &mResolutionScale, 0.25f, 1.0f);

    if (ImGui::Button("Recreate target"))
    {
        mRecreateRequested = true;
    }
}

void AOHandler::RebuildPipelines()
{
    mPipelineDeletionQueue.flush();

    mZGenPipeline = ComputePipelineBuilder("AOZBufferPipeline")
                        .SetShaderPath("assets/spirv/ao/ZGenComp.spv")
                        .AddDescriptorSetLayout(mAODescriptorSetLayout)
                        .Build(mCtx, mPipelineDeletionQueue);

    mZMipGenPipeline = ComputePipelineBuilder("AOZBufferMipPipeline")
                           .SetShaderPath("assets/spirv/ao/ZMipGenComp.spv")
                           .AddDescriptorSetLayout(mZMipGenDescriptorSetLayout)
                           .Build(mCtx, mPipelineDeletionQueue);

    mAOGenPipeline = ComputePipelineBuilder("AOGenPipeline")
                         .SetShaderPath("assets/spirv/ao/AOGenComp.spv")
                         .AddDescriptorSetLayout(mAODescriptorSetLayout)
                         .Build(mCtx, mPipelineDeletionQueue);

    mBilateralPipeline = ComputePipelineBuilder("AOBilateralPipeline")
                             .SetShaderPath("assets/spirv/ao/AOBilateralComp.spv")
                             .AddDescriptorSetLayout(mBilateralDescriptorSetLayout)
                             .Build(mCtx, mPipelineDeletionQueue);
}

void AOHandler::RecreateSwapchainResources()
{
    RecreateSwapchainResources(mResourceCache->DepthBuffer, mResourceCache->DepthOnlyView,
                               mResourceCache->DrawExtent);
}

void AOHandler::RecreateSwapchainResources(Image &depthBuffer, VkImageView depthOnlyView,
                                           VkExtent2D drawExtent)
{
    mSwapchainDeletionQueue.flush();

    // Consume the update flag:
    mRecreateRequested = false;

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

    // Create the z-buffer, ao generation target, and bilateral upscale target:
    {
        VkExtent2D targetExtent{
            .width  = ScaleResolution(drawExtent.width),
            .height = ScaleResolution(drawExtent.height),
        };

        Image2DInfo zBufferInfo{
            .Extent    = targetExtent,
            .Format    = VK_FORMAT_R32_SFLOAT,
            .Usage     = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .MipLevels = ZBufferMips,
            .Layout    = VK_IMAGE_LAYOUT_GENERAL,
        };

        mZBuffer = MakeTexture::Texture2D(mCtx, "AOZBuffer", zBufferInfo,
                                          mSwapchainDeletionQueue);

        // Create single-mip views into Z-buffer:
        for (uint32_t mip = 0; mip < ZBufferMips; mip++)
        {
            auto view = MakeView::View2D(mCtx, "AOZBufferSingleMipView",
                                         {.Img = mZBuffer.Img, .SelectLevel = mip});
            mSwapchainDeletionQueue.push_back(view);

            mZSingleLevelViews[mip] = view;
        }

        Image2DInfo aoTargetInfo{
            .Extent = targetExtent,
            .Format = VK_FORMAT_R8G8B8A8_UNORM,
            .Usage  = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .Layout = VK_IMAGE_LAYOUT_GENERAL,
        };

        mAOTarget = MakeTexture::Texture2D(mCtx, "AOTarget", aoTargetInfo,
                                           mSwapchainDeletionQueue);

        Image2DInfo bilateralInfo{
            .Extent = targetExtent,
            .Format = VK_FORMAT_R8G8B8A8_UNORM,
            .Usage  = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .Layout = VK_IMAGE_LAYOUT_GENERAL,
        };

        mBilateralTarget = MakeTexture::Texture2D(mCtx, "AOBilateralTarget",
                                                  bilateralInfo, mSwapchainDeletionQueue);
    }

    // Update Z buffer descriptor set:
    {
        // Transition depth buffer to correct layout for the update:
        mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
            auto barrierInfo = barrier::LayoutTransitionInfo{
                .Image     = depthBuffer,
                .OldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                .NewLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            };

            barrier::ImageLayoutCoarse(cmd, barrierInfo);
        });

        // Do the update:
        DescriptorUpdater(mZGenDescriptorSet)
            .WriteStorageImage(0, mZBuffer.View)
            .WriteCombinedSampler(1, depthOnlyView, mSampler)
            .Update(mCtx);

        DescriptorUpdater(mZMipGenDescriptorSet)
            .WriteStorageImages(0, mZSingleLevelViews)
            .Update(mCtx);

        // Transition z-buffer and depth buffer to correct layouts:
        mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
            auto barrierInfoZ = barrier::LayoutTransitionInfo{
                .Image     = mZBuffer.Img,
                .OldLayout = VK_IMAGE_LAYOUT_GENERAL,
                .NewLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            };
            barrier::ImageLayoutCoarse(cmd, barrierInfoZ);

            auto barrierInfo = barrier::LayoutTransitionInfo{
                .Image     = depthBuffer,
                .OldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .NewLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            };
            barrier::ImageLayoutCoarse(cmd, barrierInfo);
        });
    }

    // Update AO descriptor set:
    {
        DescriptorUpdater(mAOGenDescriptorSet)
            .WriteStorageImage(0, mAOTarget.View)
            .WriteCombinedSampler(1, mZBuffer.View, mSampler)
            .Update(mCtx);

        // Transition ao target for sampling:
        mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
            auto barrierInfoAO = barrier::LayoutTransitionInfo{
                .Image     = mAOTarget.Img,
                .OldLayout = VK_IMAGE_LAYOUT_GENERAL,
                .NewLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            };
            barrier::ImageLayoutCoarse(cmd, barrierInfoAO);
        });
    }

    // Update Bilateral descriptor set:
    {
        DescriptorUpdater(mBilateralDescriptorSet)
            .WriteStorageImage(0, mBilateralTarget.View)
            .WriteCombinedSampler(1, mAOTarget.View, mSampler)
            .WriteCombinedSampler(2, mZBuffer.View, mSampler)
            .Update(mCtx);

        // Transition target so host can update its descriptor sets:
        mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
            auto barrierInfoAO = barrier::LayoutTransitionInfo{
                .Image     = mBilateralTarget.Img,
                .OldLayout = VK_IMAGE_LAYOUT_GENERAL,
                .NewLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            };
            barrier::ImageLayoutCoarse(cmd, barrierInfoAO);
        });
    }
}

void AOHandler::RunAOPass(VkCommandBuffer cmd)
{
    // Generate the Z buffer:
    {
        barrier::TextureCompToGeneral(cmd, mZBuffer.Img);

        // Generate the 0 level of the Z-buffer:
        mZGenPipeline.Bind(cmd);
        mZGenPipeline.BindDescriptorSet(cmd, mZGenDescriptorSet, 0);

        struct PCData {
            glm::mat4 Proj;
        };

        mZGenPipeline.PushConstants(cmd, PCData{
            .Proj = mCamera.GetProj(),
        });

        mZGenPipeline.Dispatch(cmd, mZBuffer.Img.Info.extent.width, mZBuffer.Img.Info.extent.height);

        // Create higher mip-levels of the Z-buffer:
        mZMipGenPipeline.Bind(cmd);
        mZMipGenPipeline.BindDescriptorSet(cmd, mZMipGenDescriptorSet, 0);

        uint32_t resX = mZBuffer.Img.Info.extent.width / 2;
        uint32_t resY = mZBuffer.Img.Info.extent.height / 2;

        for (uint32_t mip = 1; mip < mZBuffer.Img.Info.mipLevels; mip++)
        {
            struct PCData {
                uint32_t CurrentMip;
            };
            
            mZMipGenPipeline.PushConstants(cmd, PCData{
                .CurrentMip = mip,
            });

            mZMipGenPipeline.Dispatch(cmd, resX, resY);

            // Barrier to synchronize subsequent dispatches:
            auto currentRange         = Image::GetDefaultRange(mZBuffer.Img);
            currentRange.baseMipLevel = mip;
            currentRange.levelCount   = 1;

            barrier::TextureGeneralToGeneral(cmd, mZBuffer.Img, currentRange);

            // Update next mip resolution:
            resX = std::max(1u, resX / 2);
            resY = std::max(1u, resY / 2);
        }
    }

    // Generate the AO:
    {
        barrier::TextureCompToSample(cmd, mZBuffer.Img);
        barrier::TextureCompToGeneral(cmd, mAOTarget.Img);

        // Calculate ambient occlusion:
        auto frBack = mCamera.GetFrustumBackEye();

        mAOGenPipeline.Bind(cmd);
        mAOGenPipeline.BindDescriptorSet(cmd, mAOGenDescriptorSet, 0);

        struct PCData {
            glm::vec4 TopLeft;
            glm::vec4 TopRight;
            glm::vec4 BottomLeft;
            glm::vec4 BottomRight;
        };

        mAOGenPipeline.PushConstants(cmd, PCData{
            .TopLeft     = frBack.TopLeft,
            .TopRight    = frBack.TopRight,
            .BottomLeft  = frBack.BottomLeft,
            .BottomRight = frBack.BottomRight,
        });

        mAOGenPipeline.Dispatch(cmd, mAOTarget.Img.Info.extent.width, mAOTarget.Img.Info.extent.height);
    }

    // Perform bilateral upsampling:
    {
        barrier::TextureCompToSample(cmd, mAOTarget.Img);
        barrier::TextureFragToGeneral(cmd, mBilateralTarget.Img);

        mBilateralPipeline.Bind(cmd);
        mBilateralPipeline.BindDescriptorSet(cmd, mBilateralDescriptorSet, 0);

        glm::uvec2 aoSize{mAOTarget.Img.Info.extent.width,
                          mAOTarget.Img.Info.extent.height};

        struct PCData {
            glm::uvec2 AOResolution;
            float      SigmaSpatial;
            float      SigmaVal;
        };

        mBilateralPipeline.PushConstants(cmd, PCData{
            .AOResolution = aoSize,
            .SigmaSpatial = 1.0f,
            .SigmaVal     = 0.01f,
        });

        mBilateralPipeline.Dispatch(cmd, mBilateralTarget.Img.Info.extent.width, mBilateralTarget.Img.Info.extent.height);

        barrier::TextureFragToSample(cmd, mBilateralTarget.Img);
    }
}