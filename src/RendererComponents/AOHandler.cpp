#include "AOHandler.h"
#include "Pch.h"

#include "Barrier.h"
#include "Descriptor.h"
#include "Pipeline.h"
#include "Sampler.h"
#include "VkEnums.h"

#include "imgui.h"

AOHandler::AOHandler(VulkanContext &ctx, Camera &camera, VkDescriptorPool staticPool)
    : mCtx(ctx)
    , mCamera(camera)
    , mZGenPipeline(ctx)
    , mZMipGenPipeline(ctx)
    , mAOGenPipeline(ctx)
    , mBilateralPipeline(ctx)
    , mMainDeletionQueue(ctx)
    , mSwapchainDeletionQueue(ctx)
{
    // clang-format off
    mSampler = 
        SamplerBuilder("AODepthSampler")
            .SetMagFilter(VK_FILTER_LINEAR)
            .SetMinFilter(VK_FILTER_LINEAR)
            // For screen-space depth/ao clamp to edge is
            // required to avoid ao artefacts near screen edges:
            .SetAddressMode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
            .SetMipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR)
            .SetMaxLod(12.0f)
            .Build(mCtx, mMainDeletionQueue);
    // clang-format on

    // Build the pipelines:
    RebuildPipelines();

    // Allocate the descriptor sets:
    mZGenPipeline.AllocateReflectedSets(ctx, staticPool);
    mZMipGenPipeline.AllocateReflectedSets(ctx, staticPool);
    mAOGenPipeline.AllocateReflectedSets(ctx, staticPool);
    mBilateralPipeline.AllocateReflectedSets(ctx, staticPool);
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
    ComputePipelineBuilder(mZGenPipeline)
        .SetDebugName("AOZBufferPipeline")
        .SetShaderPath("assets/spirv/ao/ZGenComp.spv")
        .Build(mCtx);

    ComputePipelineBuilder(mZMipGenPipeline)
        .SetDebugName("AOZBufferMipPipeline")
        .SetShaderPath("assets/spirv/ao/ZMipGenComp.spv")
        .Build(mCtx);

    ComputePipelineBuilder(mAOGenPipeline)
        .SetDebugName("AOGenPipeline")
        .SetShaderPath("assets/spirv/ao/AOGenComp.spv")
        .Build(mCtx);

    ComputePipelineBuilder(mBilateralPipeline)
        .SetDebugName("AOGenPipeline")
        .SetShaderPath("assets/spirv/ao/AOBilateralComp.spv")
        .Build(mCtx);
}

void AOHandler::RecreateSwapchainResources()
{
    RecreateSwapchainResources(mResourceCache->DepthBuffer, mResourceCache->DepthOnlyView, mResourceCache->DrawExtent);
}

void AOHandler::RecreateSwapchainResources(Image &depthBuffer, VkImageView depthOnlyView, VkExtent2D drawExtent)
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
        using namespace vkenum::image_usage;

        const VkExtent2D targetExtent{
            .width  = ScaleResolution(drawExtent.width),
            .height = ScaleResolution(drawExtent.height),
        };

        // clang-format off
        mZBuffer = Texture::Make2D(mCtx, {
            .Name      = "AOZBuffer",
            .Extent    = targetExtent,
            .Format    = VK_FORMAT_R32_SFLOAT,
            .Usage     = Storage | Sampled,
            .MipLevels = ZBufferMips,
            .Layout    = VK_IMAGE_LAYOUT_GENERAL,
        });

        mAOTarget = Texture::Make2D(mCtx, {
            .Name   = "AOTarget",
            .Extent = targetExtent,
            .Format = VK_FORMAT_R8G8B8A8_UNORM,
            .Usage  = Storage | Sampled,
            .Layout = VK_IMAGE_LAYOUT_GENERAL,
        });

        mBilateralTarget = Texture::Make2D(mCtx, {
            .Name   = "AOBilateralTarget",
            .Extent = targetExtent,
            .Format = VK_FORMAT_R8G8B8A8_UNORM,
            .Usage  = Storage | Sampled,
            .Layout = VK_IMAGE_LAYOUT_GENERAL,
        });
        // clang-format on
    }

    // Create single-mip views into Z-buffer:
    mZSingleLevelViews = mZBuffer->Img.MakeViewsSingleMip();

    for (auto view : mZSingleLevelViews)
    {
        mSwapchainDeletionQueue.push_back(view);
    }

    // Update Z buffer descriptor set:
    {
        // Transition depth buffer to correct layout for the update:
        mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
            using namespace vkenum::image_layout;

            barrier::ImageLayoutCoarse(cmd, {.Img = depthBuffer, .Old = DepthStencil, .New = ShaderRead});
        });

        // Do the update:
        DescriptorUpdater(mZGenPipeline.DescriptorSets.at(0))
            .WriteStorageImage(0, mZBuffer->View)
            .WriteCombinedSampler(1, depthOnlyView, mSampler)
            .Update(mCtx);

        DescriptorUpdater(mZMipGenPipeline.DescriptorSets.at(0))
            .WriteStorageImages(0, mZSingleLevelViews)
            .Update(mCtx);

        // Transition z-buffer and depth buffer to correct layouts:
        mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
            using namespace vkenum::image_layout;

            // clang-format off
            barrier::ImageLayoutCoarse(cmd, {.Img = *mZBuffer,   .Old = General,    .New = ShaderRead});
            barrier::ImageLayoutCoarse(cmd, {.Img = depthBuffer, .Old = ShaderRead, .New = DepthStencil});
            // clang-format on
        });
    }

    // Update AO descriptor set:
    {
        DescriptorUpdater(mAOGenPipeline.DescriptorSets.at(0))
            .WriteStorageImage(0, mAOTarget->View)
            .WriteCombinedSampler(1, mZBuffer->View, mSampler)
            .Update(mCtx);

        // Transition ao target for sampling:
        mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
            using namespace vkenum::image_layout;

            barrier::ImageLayoutCoarse(cmd, {.Img = *mAOTarget, .Old = General, .New = ShaderRead});
        });
    }

    // Update Bilateral descriptor set:
    {
        DescriptorUpdater(mBilateralPipeline.DescriptorSets.at(0))
            .WriteStorageImage(0, mBilateralTarget->View)
            .WriteCombinedSampler(1, mAOTarget->View, mSampler)
            .WriteCombinedSampler(2, mZBuffer->View, mSampler)
            .Update(mCtx);

        // Transition target so host can update its descriptor sets:
        mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
            using namespace vkenum::image_layout;

            barrier::ImageLayoutCoarse(cmd, {.Img = *mBilateralTarget, .Old = General, .New = ShaderRead});
        });
    }
}

void AOHandler::RunAOPass(VkCommandBuffer cmd)
{
    // Generate the Z buffer:
    {
        barrier::TextureCompToGeneral(cmd, mZBuffer->Img);

        // Generate the 0 level of the Z-buffer:
        mZGenPipeline.Bind(cmd);
        mZGenPipeline.BindReflectedSets(cmd);

        struct PCData {
            glm::mat4 Proj;
        };

        // clang-format off
        mZGenPipeline.PushConstants(cmd, PCData{
            .Proj = mCamera.GetProj(),
        });
        // clang-format on

        mZGenPipeline.Dispatch(cmd, mZBuffer->Img.Info.extent.width, mZBuffer->Img.Info.extent.height);

        // Create higher mip-levels of the Z-buffer:
        mZMipGenPipeline.Bind(cmd);
        mZMipGenPipeline.BindReflectedSets(cmd);

        uint32_t resX = mZBuffer->Img.Info.extent.width / 2;
        uint32_t resY = mZBuffer->Img.Info.extent.height / 2;

        for (uint32_t mip = 1; mip < mZBuffer->Img.Info.mipLevels; mip++)
        {
            struct PCData {
                uint32_t CurrentMip;
            };
            // clang-format off
            mZMipGenPipeline.PushConstants(cmd, PCData{
                .CurrentMip = mip,
            });
            // clang-format on

            mZMipGenPipeline.Dispatch(cmd, resX, resY);

            // Barrier to synchronize subsequent dispatches:
            auto currentRange         = mZBuffer->Img.GetDefaultRange();
            currentRange.baseMipLevel = mip;
            currentRange.levelCount   = 1;

            barrier::TextureGeneralToGeneral(cmd, mZBuffer->Img, currentRange);

            // Update next mip resolution:
            resX = std::max(1u, resX / 2);
            resY = std::max(1u, resY / 2);
        }
    }

    // Generate the AO:
    {
        barrier::TextureCompToSample(cmd, mZBuffer->Img);
        barrier::TextureCompToGeneral(cmd, mAOTarget->Img);

        // Calculate ambient occlusion:
        auto frBack = mCamera.GetFrustumBackEye();

        mAOGenPipeline.Bind(cmd);
        mAOGenPipeline.BindReflectedSets(cmd);

        struct PCData {
            glm::vec4 TopLeft;
            glm::vec4 TopRight;
            glm::vec4 BottomLeft;
            glm::vec4 BottomRight;
        };
        // clang-format off
        mAOGenPipeline.PushConstants(cmd, PCData{
            .TopLeft     = frBack.TopLeft,
            .TopRight    = frBack.TopRight,
            .BottomLeft  = frBack.BottomLeft,
            .BottomRight = frBack.BottomRight,
        });
        // clang-format on

        const auto extent = mAOTarget->Img.Info.extent;
        
        mAOGenPipeline.Dispatch(cmd, extent.width, extent.height);
    }

    // Perform bilateral upsampling:
    {
        barrier::TextureCompToSample(cmd, mAOTarget->Img);
        barrier::TextureFragToGeneral(cmd, mBilateralTarget->Img);

        mBilateralPipeline.Bind(cmd);
        mBilateralPipeline.BindReflectedSets(cmd);

        const auto aoExtent = mAOTarget->Img.Info.extent;

        struct PCData {
            glm::uvec2 AOResolution;
            float      SigmaSpatial;
            float      SigmaVal;
        };
        // clang-format off
        mBilateralPipeline.PushConstants(cmd, PCData{
            .AOResolution = {aoExtent.width, aoExtent.height},
            .SigmaSpatial = 1.0f,
            .SigmaVal     = 0.01f,
        });
        // clang-format on

        const auto extent = mBilateralTarget->Img.Info.extent;
        mBilateralPipeline.Dispatch(cmd, extent.width, extent.height);

        barrier::TextureFragToSample(cmd, mBilateralTarget->Img);
    }
}