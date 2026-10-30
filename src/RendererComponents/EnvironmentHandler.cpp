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

EnvironmentHandler::EnvironmentHandler(VulkanContext &ctx, VkDescriptorPool staticPool, BackgroundInfo info)
    : mCtx(ctx)
    , mStaticPool(staticPool)
    , mEquiRectToCubePipeline(ctx)
    , mIrradianceSHPipeline(ctx)
    , mIrradianceReducePipeline(ctx)
    , mPrefilteredGenPipeline(ctx)
    , mIntegrationGenPipeline(ctx)
    , mBackgroundPipeline(ctx)
    , mDeletionQueue(ctx)
{
    // Create the texture samplers:
    {
        // clang-format off
        mSampler = 
            SamplerBuilder("EnvSampler")
                .SetMagFilter(VK_FILTER_LINEAR)
                .SetMinFilter(VK_FILTER_LINEAR)
                .SetAddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT)
                .Build(mCtx, mDeletionQueue);

        mSamplerClamped = 
            SamplerBuilder("EnvSamplerClamped")
                .SetMagFilter(VK_FILTER_LINEAR)
                .SetMinFilter(VK_FILTER_LINEAR)
                .SetAddressMode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)
                .Build(mCtx, mDeletionQueue);

        mSamplerMipped = 
            SamplerBuilder("EnvSamplerMipped")
                .SetMagFilter(VK_FILTER_LINEAR)
                .SetMinFilter(VK_FILTER_LINEAR)
                .SetAddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT)
                .SetMipmapMode(VK_SAMPLER_MIPMAP_MODE_LINEAR)
                .SetMaxLod(12.0f)
                .Build(mCtx, mDeletionQueue);
        // clang-format on
    }

    // Create images and views:
    {
        using namespace vkenum::image_usage;

        // clang-format off
        mCubemap = Texture::MakeCube(mCtx, {
            .Name      = "EnvCubemap",
            .Extent    = {CubemapSize, CubemapSize},
            .Format    = VK_FORMAT_R32G32B32A32_SFLOAT,
            .Usage     = TransferSrc | TransferDst | Storage | Sampled,
            .MipLevels = Image::MipCount(CubemapSize),
            .Layout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        });

        mPrefiltered = Texture::MakeCube(mCtx, {
            .Name      = "EnvPrefilteredMap",
            .Extent    = {PrefilteredSize, PrefilteredSize},
            .Format    = VK_FORMAT_R32G32B32A32_SFLOAT,
            .Usage     = TransferDst | Storage | Sampled,
            .MipLevels = Image::MipCount(PrefilteredSize),
            .Layout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        });

        mIntegration = Texture::Make2D(mCtx, {
            .Name   = "EnvIntegrationMap",
            .Extent = {IntegrationSize, IntegrationSize},
            .Format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .Usage  = Storage | Sampled,
        });
        // clang-format on

        // Create single mip prefiltered views for usage in compute:
        mPrefilteredMipViews = mPrefiltered->Img.MakeViewsSingleMip();

        for (auto view : mPrefilteredMipViews)
        {
            mDeletionQueue.push_back(view);
        }
    }

    // Create buffers:
    {
        using namespace vkenum::buffer_usage;

        // Create uniform buffer for environment information (light direction etc.):
        {
            // clang-format off
            mEnvUBO = Buffer::MakeMappedUniform(ctx, {
                .Name = "EnvLightUniformBuffer",
                .Size = sizeof(mEnvUBOData),
            });
            // clang-format on

            mEnvUBO->UploadMapped(&mEnvUBOData, sizeof(mEnvUBOData));
        }

        // Create shader storage buffers for computing irradiance SH decomposition:
        {
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

            const uint32_t groupsPerLine = (CubemapSize / mReduceBlock);
            const uint32_t groupsPerSide = groupsPerLine * groupsPerLine;

            mFirstBufferLen = 6 * groupsPerSide;

            // clang-format off
            mFirstReducionBuffer = Buffer(ctx, {
                .Name  = "EnvFirstReductionBuffer",
                .Size  = mFirstBufferLen * sizeof(SHData),
                .Usage = StorageBuffer,
            });

            mFinalReductionBuffer = Buffer(ctx, {
                .Name  = "EnvFinalReductionBuffer",
                .Size  = 1 * sizeof(SHData),
                .Usage = StorageBuffer | TransferDst,
            });
            // clang-format on
        }
    }

    // Create the manually managed descriptor set layout:
    // clang-format off
    mLightingDescriptorSetLayout = 
        DescriptorSetLayoutBuilder("EnvLightingDescriptorLayout")
            .AddUniformBuffer(0, VK_SHADER_STAGE_FRAGMENT_BIT)
            .AddStorageBuffer(1, VK_SHADER_STAGE_FRAGMENT_BIT)
            .AddCombinedSampler(2, VK_SHADER_STAGE_FRAGMENT_BIT)
            .AddCombinedSampler(3, VK_SHADER_STAGE_FRAGMENT_BIT)
            .Build(ctx, mDeletionQueue);
    // clang-format on

    // Rebuild the pipelines:
    RebuildPipelines(info);

    // Allocate and update descriptor sets:
    {
        mLightingDescriptorSet = Descriptor::Allocate(mCtx, staticPool, mLightingDescriptorSetLayout);

        DescriptorUpdater(mLightingDescriptorSet)
            .WriteUniformBuffer(0, mEnvUBO->Handle, sizeof(mEnvUBOData))
            .WriteStorageBuffer(1, mFinalReductionBuffer->Handle, mFinalReductionBuffer->Size)
            .WriteCombinedSampler(2, mPrefiltered->View, mSamplerMipped)
            .WriteCombinedSampler(3, mIntegration->View, mSamplerClamped)
            .Update(mCtx);
    }
    {
        mIrradianceSHPipeline.AllocateReflectedSets(mCtx, mStaticPool);

        DescriptorUpdater(mIrradianceSHPipeline.DescriptorSets.at(0))
            .WriteCombinedSampler(0, mCubemap->View, mSampler)
            .Update(mCtx);

        DescriptorUpdater(mIrradianceSHPipeline.DescriptorSets.at(1))
            .WriteStorageBuffer(0, mFirstReducionBuffer->Handle, mFirstReducionBuffer->Size)
            .Update(mCtx);
    }
    {
        mIrradianceReducePipeline.AllocateReflectedSets(mCtx, mStaticPool);

        DescriptorUpdater(mIrradianceReducePipeline.DescriptorSets.at(0))
            .WriteStorageBuffer(0, mFirstReducionBuffer->Handle, mFirstReducionBuffer->Size)
            .WriteStorageBuffer(1, mFinalReductionBuffer->Handle, mFinalReductionBuffer->Size)
            .Update(mCtx);
    }
    {
        mBackgroundPipeline.AllocateReflectedSets(mCtx, mStaticPool);

        DescriptorUpdater(mBackgroundPipeline.DescriptorSets.at(0))
            .WriteCombinedSampler(0, mCubemap->View, mSampler)
            .Update(mCtx);
    }
    {
        mIntegrationGenPipeline.AllocateReflectedSets(mCtx, mStaticPool);

        DescriptorUpdater(mIntegrationGenPipeline.DescriptorSets.at(0))
            .WriteStorageImage(0, mIntegration->View)
            .Update(mCtx);
    }

    mPrefilteredGenPipeline.AllocateReflectedSets(mCtx, mStaticPool);

    // Nothing to update yet - waiting for image:
    mEquiRectToCubePipeline.AllocateReflectedSets(mCtx, mStaticPool);

    // Generate the integration map once: (it only depends on the chosen BRDF,
    // and we have only one option atm)
    GenerateIntegrationMap();
}

void EnvironmentHandler::RebuildPipelines(BackgroundInfo info)
{
    ComputePipelineBuilder(mEquiRectToCubePipeline)
        .SetDebugName("EnvEquToCubePipeline")
        .SetShaderPath("assets/spirv/environment/EquiToCubeComp.spv")
        .Build(mCtx);

    ComputePipelineBuilder(mIrradianceSHPipeline)
        .SetDebugName("EnvIrradianceSHPipeline")
        .SetShaderPath("assets/spirv/environment/IrradianceCalcSHComp.spv")
        .Build(mCtx);

    ComputePipelineBuilder(mIrradianceReducePipeline)
        .SetDebugName("EnvIrradianceReducePipeline")
        .SetShaderPath("assets/spirv/environment/IrradianceReduceComp.spv")
        .Build(mCtx);

    ComputePipelineBuilder(mPrefilteredGenPipeline)
        .SetDebugName("EnvPrefilteredGenPipeline")
        .SetShaderPath("assets/spirv/environment/PrefilteredGenComp.spv")
        .Build(mCtx);

    ComputePipelineBuilder(mIntegrationGenPipeline)
        .SetDebugName("EnvIntegrationPipeline")
        .SetShaderPath("assets/spirv/environment/IntegrationGenComp.spv")
        .Build(mCtx);

    GraphicsPipelineBuilder(mBackgroundPipeline)
        .SetDebugName("EnvBackgroundPipeline")
        .SetShaderPathVertex("assets/spirv/environment/BackgroundVert.spv")
        .SetShaderPathFragment("assets/spirv/environment/BackgroundFrag.spv")
        // No vertex format, since we just hardcode the fullscreen
        // triangle in the vertex shader:
        .SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetPolygonMode(VK_POLYGON_MODE_FILL)
        .SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .SetColorFormat(info.ColorFormat)
        .EnableDepthTest(VK_COMPARE_OP_LESS_OR_EQUAL)
        .SetDepthFormat(info.DepthFormat)
        .SetStencilFormat(info.DepthFormat)
        .SetMultisampling(info.SampleCount)
        .Build(mCtx);
}

void EnvironmentHandler::LoadEnvironment(const Scene &scene)
{
    const auto maxPrefilteredLod = static_cast<float>(std::log2(mPrefiltered->Img.Info.extent.width));

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

void EnvironmentHandler::DrawBackground(VkCommandBuffer cmd, FrustumBack frustumBack, VkExtent2D drawExtent)
{
    if (!HdriEnabled())
        return;

    mBackgroundPipeline.Bind(cmd);
    common::ViewportScissor(cmd, drawExtent);

    // mBackgroundPipeline.BindDescriptorSet(cmd, mBackgroundDescriptorSet, 0);
    mBackgroundPipeline.BindReflectedSets(cmd);
    mBackgroundPipeline.PushConstants(cmd, frustumBack);

    vkCmdDraw(cmd, 3, 1, 0, 0);
}

void EnvironmentHandler::ConvertEquirectToCubemap(const ImageData &data)
{
    vassert(data.Format == VK_FORMAT_R32G32B32A32_SFLOAT);

    // Load equirectangular environment map:
    auto envMap = Texture::FromData(mCtx, "EnvEnvironmentMap", data);

    DescriptorUpdater(mEquiRectToCubePipeline.DescriptorSets.at(0))
        .WriteStorageImage(0, mCubemap->View)
        .WriteCombinedSampler(1, envMap.View, mSampler)
        .Update(mCtx);

    mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
        using namespace vkenum::image_layout;

        // Transition cubemap to use as storage image:
        barrier::ImageLayoutCoarse(cmd, {.Img = *mCubemap, .Old = Undefined, .New = General});

        // Sample equirectangular map to cubemap
        // using a compute pipeline:
        mEquiRectToCubePipeline.Bind(cmd);
        mEquiRectToCubePipeline.BindReflectedSets(cmd);

        auto extent = mCubemap->Img.Info.extent;
        mEquiRectToCubePipeline.Dispatch(cmd, extent.width, extent.height, 6);

        // Transition cubemap back to be used as texture:
        barrier::ImageLayoutCoarse(cmd, {.Img = *mCubemap, .Old = General, .New = ShaderRead});
    });

    // Generate mip levels (to use when generating prefiltered map)
    mCubemap->Img.GenerateMips();
}

void EnvironmentHandler::CalculateDiffuseIrradiance()
{
    // Do parallel patch-based computation of SH coeficcients
    mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
        mIrradianceSHPipeline.Bind(cmd);
        mIrradianceSHPipeline.BindReflectedSets(cmd);

        struct PCData {
            uint32_t CubemapRes;
            uint32_t ReduceBlock;
        };

        // clang-format off
        mIrradianceSHPipeline.PushConstants(cmd, PCData{
            .CubemapRes  = mCubemap->Img.Info.extent.width,
            .ReduceBlock = mReduceBlock,
        });
        // clang-format on

        mIrradianceSHPipeline.Dispatch(cmd, mFirstBufferLen);
    });

    // Sum-reduce the resulting array:
    mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
        mIrradianceReducePipeline.Bind(cmd);
        mIrradianceReducePipeline.BindReflectedSets(cmd);

        mIrradianceReducePipeline.PushConstants(cmd, mFirstBufferLen);

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
        // clang-format off
        barrier::ImageLayoutCoarse(cmd, {.Img = *mCubemap,     .Old = ShaderRead, .New = TransferSrc});
        barrier::ImageLayoutCoarse(cmd, {.Img = *mPrefiltered, .Old = Undefined,  .New = TransferDst});

        // Issue the blit command:
        vkutils::BlitImageZeroMip(cmd, *mCubemap, *mPrefiltered);

        // Transition: cubemap -> texture, prefiltered -> General:
        barrier::ImageLayoutCoarse(cmd, {.Img = *mCubemap,     .Old = TransferSrc, .New = ShaderRead});
        barrier::ImageLayoutCoarse(cmd, {.Img = *mPrefiltered, .Old = TransferDst, .New = General});
        // clang-format on
    });

    // Generate higher mips by integrating cubemap
    // over increasingly larger lobes.
    {
        const auto extent = mPrefiltered->Img.Info.extent;

        uint32_t resX = extent.width / 2;
        uint32_t resY = extent.height / 2;

        for (uint32_t mip = 1; mip < numMips; mip++)
        {
            const float roughness = static_cast<float>(mip) / static_cast<float>(numMips - 1);

            // Update prefiltered descriptor to access appropriate mip level
            DescriptorUpdater(mPrefilteredGenPipeline.DescriptorSets.at(0))
                .WriteCombinedSampler(0, mCubemap->View, mSamplerMipped)
                .WriteStorageImage(1, mPrefilteredMipViews[mip])
                .Update(mCtx);

            mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
                mPrefilteredGenPipeline.Bind(cmd);
                mPrefilteredGenPipeline.BindReflectedSets(cmd);

                struct PCData {
                    uint32_t CubeResolution;
                    uint32_t MipLevel;
                    float    Roughness;
                };

                // clang-format off
                mPrefilteredGenPipeline.PushConstants(cmd, PCData{
                    .CubeResolution = mCubemap->Img.Info.extent.width,
                    .MipLevel       = mip,
                    .Roughness      = roughness,
                });
                // clang-format on

                mPrefilteredGenPipeline.Dispatch(cmd, resX, resY, 6);
            });

            resX = std::max(1u, resX / 2);
            resY = std::max(1u, resY / 2);
        }
    }

    // Transition prefiltered map to be used as a texture:
    mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
        using namespace vkenum::image_layout;

        barrier::ImageLayoutCoarse(cmd, {.Img = *mPrefiltered, .Old = General, .New = ShaderRead});
    });
}

void EnvironmentHandler::GenerateIntegrationMap()
{
    mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
        using namespace vkenum::image_layout;

        barrier::ImageLayoutCoarse(cmd, {.Img = *mIntegration, .Old = Undefined, .New = General});

        mIntegrationGenPipeline.Bind(cmd);
        mIntegrationGenPipeline.BindReflectedSets(cmd);

        auto extent = mIntegration->Img.Info.extent;
        mIntegrationGenPipeline.Dispatch(cmd, extent.width, extent.height);

        barrier::ImageLayoutCoarse(cmd, {.Img = *mIntegration, .Old = General, .New = ShaderRead});
    });
}

void EnvironmentHandler::ResetToBlack()
{
    mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
        auto ResetImageToBlack = [&](Image &img) {
            using namespace vkenum::image_layout;

            barrier::ImageLayoutCoarse(cmd, {.Img = img, .Old = Undefined, .New = TransferDst});

            auto range = img.GetDefaultRange();

            VkClearColorValue color = {
                {0.0f, 0.0f, 0.0f, 1.0f}
            };

            vkCmdClearColorImage(cmd, img.Handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &range);

            barrier::ImageLayoutCoarse(cmd, {.Img = img, .Old = TransferDst, .New = ShaderRead});
        };

        // Clear cubemap and prefiltered map to pure black:
        ResetImageToBlack(*mCubemap);
        ResetImageToBlack(*mPrefiltered);

        // Set all SH coefficients to 0:
        auto fillValue = std::bit_cast<uint32_t, float>(0.0f);

        vkCmdFillBuffer(cmd, mFinalReductionBuffer->Handle, 0, mFinalReductionBuffer->Size, fillValue);
    });
}