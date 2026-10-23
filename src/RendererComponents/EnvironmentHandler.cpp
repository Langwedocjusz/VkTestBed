#include "EnvironmentHandler.h"
#include "Pch.h"

#include "Barrier.h"
#include "Common.h"
#include "Descriptor.h"
#include "Pipeline.h"
#include "Sampler.h"
#include "Vassert.h"
#include "VkEnums.h"
#include "VkUtils.h"

#include "volk.h"
#include <glm/gtc/matrix_transform.hpp>

#include <bit>
#include <cmath>

EnvironmentHandler::EnvironmentHandler(VulkanContext &ctx, VkDescriptorPool staticPool)
    : mCtx(ctx), mDeletionQueue(ctx), mPipelineDeletionQueue(ctx)
{
    // Create the texture samplers:
    {
        mSampler = SamplerBuilder("EnvSampler")
                       .SetMagFilter(VK_FILTER_LINEAR)
                       .SetMinFilter(VK_FILTER_LINEAR)
                       .SetAddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT)
                       .Build(mCtx, mDeletionQueue);

        mSamplerClamped = SamplerBuilder("EnvSamplerClamped")
                              .SetMagFilter(VK_FILTER_LINEAR)
                              .SetMinFilter(VK_FILTER_LINEAR)
                              .SetAddressMode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
                              .Build(mCtx, mDeletionQueue);

        mSamplerMipped = SamplerBuilder("EnvSamplerMipped")
                             .SetMagFilter(VK_FILTER_LINEAR)
                             .SetMinFilter(VK_FILTER_LINEAR)
                             .SetAddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT)
                             .SetMipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR)
                             .SetMaxLod(12.0f)
                             .Build(mCtx, mDeletionQueue);
    }

    // Create images and views:
    {
        using namespace vkenum::image_usage;

        mCubemap = Texture::MakeCube(
            mCtx, {
                      .Name      = "EnvCubemap",
                      .Extent    = {CubemapSize, CubemapSize},
                      .Format    = VK_FORMAT_R32G32B32A32_SFLOAT,
                      .Usage     = TransferSrc | TransferDst | Storage | Sampled,
                      .MipLevels = Image::MipCount(CubemapSize),
                      .Layout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        });

        mPrefiltered = Texture::MakeCube(
            mCtx, {
                      .Name      = "EnvPrefilteredMap",
                      .Extent    = {PrefilteredSize, PrefilteredSize},
                      .Format    = VK_FORMAT_R32G32B32A32_SFLOAT,
                      .Usage     = TransferDst | Storage | Sampled,
                      .MipLevels = Image::MipCount(PrefilteredSize),
                      .Layout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        });

        mIntegration =
            Texture::Make2D(mCtx, {
                                      .Name   = "EnvIntegrationMap",
                                      .Extent = {IntegrationSize, IntegrationSize},
                                      .Format = VK_FORMAT_R32G32B32A32_SFLOAT,
                                      .Usage  = Storage | Sampled,
        });

        // Create single mip prefiltered views for usage in compute:
        for (uint32_t mip = 0; mip < mPrefiltered->Img.Info.mipLevels; mip++)
        {
            auto view = mPrefiltered->Img.MakeView({
                .Name        = "EnvPrefilteredViewMip" + std::to_string(mip),
                .SelectLevel = mip,
            });

            mPrefilteredMipViews.push_back(view);
            mDeletionQueue.push_back(view);
        }
    }

    // Create buffers:
    {
        using namespace vkenum::buffer_usage;

        // Create uniform buffer for environment information (light direction etc.):
        {
            Buffer::ThinInfo uboInfo{
                .Name = "EnvLightUniformBuffer",
                .Size = sizeof(mEnvUBOData),
            };
            mEnvUBO.emplace(Buffer::MakeMappedUniform(ctx, uboInfo));

            mEnvUBO->UploadMapped(&mEnvUBOData, sizeof(mEnvUBOData));
        }

        // Create shader storage buffers for computing irradiance SH decomposition:
        {
            const uint32_t groupsPerLine = (CubemapSize / mReduceBlock);
            const uint32_t groupsPerSide = groupsPerLine * groupsPerLine;

            mFirstBufferLen = 6 * groupsPerSide;

            struct SHData {
                glm::vec4 SH_L0;
                glm::vec4 SH_L1_M_1;
                glm::vec4 SH_L1_M0;
                glm::vec4 SH_L1_M1;
                glm::vec4 SH_L2_M_2;
                glm::vec4 SH_L2_M_1;
                glm::vec4 SH_L2_M0;
                glm::vec4 SH_L2_M1;
                glm::vec4 SH_L2_M2;
            };

            {
                Buffer::Info info{
                    .Name  = "EnvFirstReductionBuffer",
                    .Size  = mFirstBufferLen * sizeof(SHData),
                    .Usage = StorageBuffer,
                };

                mFirstReducionBuffer.emplace(ctx, info);
            }

            {
                Buffer::Info info{
                    .Name  = "EnvFinalReductionBuffer",
                    .Size  = 1 * sizeof(SHData),
                    .Usage = StorageBuffer | TransferDst,
                };

                mFinalReductionBuffer.emplace(ctx, info);
            }
        }
    }

    // Initialize descriptor layouts:
    mBackgroundDescrptorSetLayout =
        DescriptorSetLayoutBuilder("EnvBackgroundDescriptorLayout")
            .AddCombinedSampler(0, VK_SHADER_STAGE_FRAGMENT_BIT |
                                       VK_SHADER_STAGE_COMPUTE_BIT)
            .Build(ctx, mDeletionQueue);

    mLightingDescriptorSetLayout =
        DescriptorSetLayoutBuilder("EnvLightingDescriptorLayout")
            .AddUniformBuffer(0, VK_SHADER_STAGE_FRAGMENT_BIT)
            .AddStorageBuffer(1, VK_SHADER_STAGE_FRAGMENT_BIT)
            .AddCombinedSampler(2, VK_SHADER_STAGE_FRAGMENT_BIT)
            .AddCombinedSampler(3, VK_SHADER_STAGE_FRAGMENT_BIT)
            .Build(ctx, mDeletionQueue);

    mTexToImgDescriptorSetLayout =
        DescriptorSetLayoutBuilder("EnvTexToImgDescriptorLayout")
            .AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT)
            .AddCombinedSampler(1, VK_SHADER_STAGE_COMPUTE_BIT)
            .Build(ctx, mDeletionQueue);

    mIrradianceDescriptorSetLayout =
        DescriptorSetLayoutBuilder("EnvIrradianceDescriptorLayout")
            .AddStorageBuffer(0, VK_SHADER_STAGE_COMPUTE_BIT)
            .AddStorageBuffer(1, VK_SHADER_STAGE_COMPUTE_BIT)
            .Build(ctx, mDeletionQueue);

    mPrefilteredDescriptorSetLayout =
        DescriptorSetLayoutBuilder("EnvPrefilteredDescriptorLayout")
            .AddCombinedSampler(0, VK_SHADER_STAGE_COMPUTE_BIT)
            .AddStorageImage(1, VK_SHADER_STAGE_COMPUTE_BIT)
            .Build(ctx, mDeletionQueue);

    mIntegrationDescriptorSetLayout =
        DescriptorSetLayoutBuilder("EnvIntegrationDescriptorLayout")
            .AddStorageImage(0, VK_SHADER_STAGE_COMPUTE_BIT)
            .Build(mCtx, mDeletionQueue);

    // Allocate and update descriptor sets:

    // Descriptor set for sampling the background cubemap
    mBackgroundDescriptorSet =
        Descriptor::Allocate(mCtx, staticPool, mBackgroundDescrptorSetLayout);

    DescriptorUpdater(mBackgroundDescriptorSet)
        .WriteCombinedSampler(0, mCubemap->View, mSampler)
        .Update(mCtx);

    // Descriptor set for using lighting information:
    mLightingDescriptorSet =
        Descriptor::Allocate(mCtx, staticPool, mLightingDescriptorSetLayout);

    DescriptorUpdater(mLightingDescriptorSet)
        .WriteUniformBuffer(0, mEnvUBO->Handle, sizeof(mEnvUBOData))
        .WriteStorageBuffer(1, mFinalReductionBuffer->Handle, mFinalReductionBuffer->Size)
        .WriteCombinedSampler(2, mPrefiltered->View, mSamplerMipped)
        .WriteCombinedSampler(3, mIntegration->View, mSamplerClamped)
        .Update(mCtx);

    // Descriptor set for sampling a texure and saving to image:
    mTexToImgDescriptorSet =
        Descriptor::Allocate(mCtx, staticPool, mTexToImgDescriptorSetLayout);
    // Nothing to update yet - waiting for image.

    // Descriptor set for irradiance SH data buffer:
    mIrradianceDescriptorSet =
        Descriptor::Allocate(mCtx, staticPool, mIrradianceDescriptorSetLayout);

    DescriptorUpdater(mIrradianceDescriptorSet)
        .WriteStorageBuffer(0, mFirstReducionBuffer->Handle, mFirstReducionBuffer->Size)
        .WriteStorageBuffer(1, mFinalReductionBuffer->Handle, mFinalReductionBuffer->Size)
        .Update(mCtx);

    // Descriptor set for generation of prefiltered map:
    mPrefilteredDescriptorSet =
        Descriptor::Allocate(mCtx, staticPool, mPrefilteredDescriptorSetLayout);
    // Nothing to update - yet - will be done while generating subsequent levels

    // Descriptor set for generating the integration map:
    mIntegrationDescriptorSet =
        Descriptor::Allocate(mCtx, staticPool, mIntegrationDescriptorSetLayout);

    DescriptorUpdater(mIntegrationDescriptorSet)
        .WriteStorageImage(0, mIntegration->View)
        .Update(mCtx);
}

void EnvironmentHandler::RebuildPipelines(VkFormat colorFormat, VkFormat depthFormat,
                                          VkSampleCountFlagBits sampleCount)
{
    mPipelineDeletionQueue.flush();

    mEquiRectToCubePipeline =
        ComputePipelineBuilder("EnvEquToCubePipeline")
            .SetShaderPath("assets/spirv/environment/EquiToCubeComp.spv")
            .AddDescriptorSetLayout(mTexToImgDescriptorSetLayout)
            .Build(mCtx, mPipelineDeletionQueue);

    mIrradianceSHPipeline =
        ComputePipelineBuilder("EnvIrradianceSHPipeline")
            .SetShaderPath("assets/spirv/environment/IrradianceCalcSHComp.spv")
            .AddDescriptorSetLayout(mBackgroundDescrptorSetLayout)
            .AddDescriptorSetLayout(mIrradianceDescriptorSetLayout)
            .Build(mCtx, mPipelineDeletionQueue);

    mIrradianceReducePipeline =
        ComputePipelineBuilder("EnvIrradianceReducePipeline")
            .SetShaderPath("assets/spirv/environment/IrradianceReduceComp.spv")
            .AddDescriptorSetLayout(mIrradianceDescriptorSetLayout)
            .Build(mCtx, mPipelineDeletionQueue);

    mPrefilteredGenPipeline =
        ComputePipelineBuilder("EnvPrefilteredGenPipeline")
            .SetShaderPath("assets/spirv/environment/PrefilteredGenComp.spv")
            .AddDescriptorSetLayout(mPrefilteredDescriptorSetLayout)
            .Build(mCtx, mPipelineDeletionQueue);

    mIntegrationGenPipeline =
        ComputePipelineBuilder("EnvIntegrationPipeline")
            .SetShaderPath("assets/spirv/environment/IntegrationGenComp.spv")
            .AddDescriptorSetLayout(mIntegrationDescriptorSetLayout)
            .Build(mCtx, mPipelineDeletionQueue);

    // Generate the integration map once (it is cubemap independent):
    if (!mIntegrationGenerated)
    {
        GenerateIntegrationMap();
        mIntegrationGenerated = false;
    }

    mBackgroundPipeline =
        PipelineBuilder("EnvBackgroundPipeline")
            .SetShaderPathVertex("assets/spirv/environment/BackgroundVert.spv")
            .SetShaderPathFragment("assets/spirv/environment/BackgroundFrag.spv")
            // No vertex format, since we just hardcode the fullscreen
            // triangle in the vertex shader:
            .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetPolygonMode(VK_POLYGON_MODE_FILL)
            .SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .SetColorFormat(colorFormat)
            .AddDescriptorSetLayout(mBackgroundDescrptorSetLayout)
            .EnableDepthTest(VK_COMPARE_OP_LESS_OR_EQUAL)
            .SetDepthFormat(depthFormat)
            .SetStencilFormat(depthFormat)
            .SetMultisampling(sampleCount)
            .Build(mCtx, mPipelineDeletionQueue);
}

void EnvironmentHandler::LoadEnvironment(const Scene &scene)
{
    const auto maxPrefilteredLod =
        static_cast<float>(std::log2(mPrefiltered->Img.Info.extent.width));

    mEnvUBOData = EnvUBOData{
        .LightOn          = static_cast<int32_t>(scene.Env.DirLightOn),
        .LightDir         = scene.Env.LightDir,
        .LightColor       = scene.Env.LightColor,
        .HdriEnabled      = scene.Env.HdriImage.has_value(),
        .MaxReflectionLod = maxPrefilteredLod,
    };

    mEnvUBO->UploadMapped(&mEnvUBOData, sizeof(mEnvUBOData));

    if (scene.Env.ReloadImage)
    {
        if (mEnvUBOData.HdriEnabled)
        {
            ConvertEquirectToCubemap(*scene.Env.HdriImage);
            CalculateDiffuseIrradiance();
            GeneratePrefilteredMap();
        }

        else
        {
            ResetToBlack();
        }
    }

    scene.Env.ReloadImage = false;
}

void EnvironmentHandler::DrawBackground(VkCommandBuffer cmd, FrustumBack frustumBack,
                                        VkExtent2D drawExtent)
{
    if (!HdriEnabled())
        return;

    mBackgroundPipeline.Bind(cmd);
    common::ViewportScissor(cmd, drawExtent);

    mBackgroundPipeline.BindDescriptorSet(cmd, mBackgroundDescriptorSet, 0);
    mBackgroundPipeline.PushConstants(cmd, frustumBack);

    vkCmdDraw(cmd, 3, 1, 0, 0);
}

void EnvironmentHandler::ConvertEquirectToCubemap(const ImageData &data)
{
    vassert(data.Format == VK_FORMAT_R32G32B32A32_SFLOAT);

    // Load equirectangular environment map:
    auto envMap = Texture::FromData(mCtx, "EnvEnvironmentMap", data);

    DescriptorUpdater(mTexToImgDescriptorSet)
        .WriteStorageImage(0, mCubemap->View)
        .WriteCombinedSampler(1, envMap.View, mSampler)
        .Update(mCtx);

    mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
        using namespace vkenum::image_layout;

        // Transition cubemap to use as storage image:
        barrier::ImageLayoutCoarse(cmd,
                                   {.Img = *mCubemap, .Old = Undefined, .New = General});

        // Sample equirectangular map to cubemap
        // using a compute pipeline:
        mEquiRectToCubePipeline.Bind(cmd);
        mEquiRectToCubePipeline.BindDescriptorSet(cmd, mTexToImgDescriptorSet, 0);

        auto extent = mCubemap->Img.Info.extent;
        mEquiRectToCubePipeline.Dispatch(cmd, extent.width, extent.height, 6);

        // Transition cubemap back to be used as texture:
        barrier::ImageLayoutCoarse(
            cmd, {.Img = *mCubemap, .Old = General, .New = ShaderReadOnly});
    });

    // Generate mip levels (to use when generating prefiltered map)
    mCubemap->Img.GenerateMips();
}

void EnvironmentHandler::CalculateDiffuseIrradiance()
{
    // Do parallel patch-based computation of SH coeficcients
    mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
        std::array descriptorSets{
            mBackgroundDescriptorSet,
            mIrradianceDescriptorSet,
        };

        mIrradianceSHPipeline.Bind(cmd);
        mIrradianceSHPipeline.BindDescriptorSets(cmd, descriptorSets, 0);

        struct PCData {
            uint32_t CubemapRes;
            uint32_t ReduceBlock;
        };

        mIrradianceSHPipeline.PushConstants(
            cmd, PCData{
                     .CubemapRes  = mCubemap->Img.Info.extent.width,
                     .ReduceBlock = mReduceBlock,
                 });

        mIrradianceSHPipeline.Dispatch(cmd, mFirstBufferLen);
    });

    // Sum-reduce the resulting array:
    mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
        mIrradianceReducePipeline.Bind(cmd);
        mIrradianceReducePipeline.BindDescriptorSet(cmd, mIrradianceDescriptorSet, 0);

        struct PCData {
            uint32_t BufferSize;
        };

        mIrradianceReducePipeline.PushConstants(cmd, PCData{
                                                         .BufferSize = mFirstBufferLen,
                                                     });

        // Defaults to (1,1,1) threads:
        mIrradianceReducePipeline.Dispatch(cmd);
    });
}

void EnvironmentHandler::GeneratePrefilteredMap()
{
    const auto numMips = mPrefiltered->Img.Info.mipLevels;

    // Blit cubemap onto prefiltered map level zero:
    mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
        using namespace vkenum::image_layout;

        // Transition: cubemap -> TransferSrc, prefiltered -> TransferDst:
        barrier::ImageLayoutCoarse(
            cmd, {.Img = *mCubemap, .Old = ShaderReadOnly, .New = TransferSrc});

        barrier::ImageLayoutCoarse(
            cmd, {.Img = *mPrefiltered, .Old = Undefined, .New = TransferDst});

        // Issue the blit command:
        vkutils::BlitImageZeroMip(cmd, mCubemap->Img, mPrefiltered->Img);

        // Transition: cubemap -> texture, prefiltered -> General:
        barrier::ImageLayoutCoarse(
            cmd, {.Img = *mCubemap, .Old = TransferSrc, .New = ShaderReadOnly});

        barrier::ImageLayoutCoarse(
            cmd, {.Img = *mPrefiltered, .Old = TransferDst, .New = General});
    });

    // Generate higher mips by integrating cubemap
    // over increasingly larger lobes.
    {
        uint32_t resX = mPrefiltered->Img.Info.extent.width / 2;
        uint32_t resY = mPrefiltered->Img.Info.extent.height / 2;

        for (uint32_t mip = 1; mip < numMips; mip++)
        {
            const float roughness =
                static_cast<float>(mip) / static_cast<float>(numMips - 1);

            // Update prefiltered descriptor to access appropriate mip level
            DescriptorUpdater(mPrefilteredDescriptorSet)
                .WriteCombinedSampler(0, mCubemap->View, mSamplerMipped)
                .WriteStorageImage(1, mPrefilteredMipViews[mip])
                .Update(mCtx);

            mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
                mPrefilteredGenPipeline.Bind(cmd);
                mPrefilteredGenPipeline.BindDescriptorSet(cmd, mPrefilteredDescriptorSet,
                                                          0);

                struct PCData {
                    uint32_t CubeResolution;
                    uint32_t MipLevel;
                    float    Roughness;
                };

                mPrefilteredGenPipeline.PushConstants(
                    cmd, PCData{
                             .CubeResolution = mCubemap->Img.Info.extent.width,
                             .MipLevel       = mip,
                             .Roughness      = roughness,
                         });

                mPrefilteredGenPipeline.Dispatch(cmd, resX, resY, 6);
            });

            resX = std::max(1u, resX / 2);
            resY = std::max(1u, resY / 2);
        }
    }

    // Transition prefiltered map to be used as a texture:
    mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
        using namespace vkenum::image_layout;

        barrier::ImageLayoutCoarse(
            cmd, {.Img = *mPrefiltered, .Old = General, .New = ShaderReadOnly});
    });
}

void EnvironmentHandler::GenerateIntegrationMap()
{
    mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
        using namespace vkenum::image_layout;

        barrier::ImageLayoutCoarse(
            cmd, {.Img = *mIntegration, .Old = Undefined, .New = General});

        mIntegrationGenPipeline.Bind(cmd);
        mIntegrationGenPipeline.BindDescriptorSet(cmd, mIntegrationDescriptorSet, 0);

        auto extent = mIntegration->Img.Info.extent;
        mIntegrationGenPipeline.Dispatch(cmd, extent.width, extent.height);

        barrier::ImageLayoutCoarse(
            cmd, {.Img = *mIntegration, .Old = General, .New = ShaderReadOnly});
    });
}

void EnvironmentHandler::ResetToBlack()
{
    mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
        auto clearImage = [&](Image &img) {
            using namespace vkenum::image_layout;

            barrier::ImageLayoutCoarse(
                cmd, {.Img = img, .Old = Undefined, .New = TransferDst});

            VkClearColorValue black = {
                {0.0f, 0.0f, 0.0f, 1.0f}
            };

            auto range = VkImageSubresourceRange{
                .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel   = 0,
                .levelCount     = img.Info.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = img.Info.arrayLayers,
            };

            vkCmdClearColorImage(cmd, img.Handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                 &black, 1, &range);

            barrier::ImageLayoutCoarse(
                cmd, {.Img = img, .Old = TransferDst, .New = ShaderReadOnly});
        };

        // Clear cubemap and prefiltered map to pure black:
        clearImage(mCubemap->Img);
        clearImage(mPrefiltered->Img);

        // Set all SH coefficients to 0:
        auto fillValue = std::bit_cast<uint32_t, float>(0.0f);

        vkCmdFillBuffer(cmd, mFinalReductionBuffer->Handle, 0,
                        mFinalReductionBuffer->Size, fillValue);
    });
}