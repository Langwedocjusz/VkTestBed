#include "EnvironmentHandler.h"
#include "Pch.h"

#include "Barrier.h"
#include "BufferUtils.h"
#include "Descriptor.h"
#include "ImageLoaders.h"
#include "ImageUtils.h"
#include "Pipeline.h"
#include "Sampler.h"
#include "VkUtils.h"

#include <vulkan/vulkan.h>

#include <cmath>
#include <vulkan/vulkan_core.h>

EnvironmentHandler::EnvironmentHandler(VulkanContext &ctx)
    : mCtx(ctx), mDescriptorAllocator(ctx), mDeletionQueue(ctx),
      mPipelineDeletionQueue(ctx)
{
    // Create the texture samplers:
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

    // Initialize main descriptor allocator:
    {
        std::vector<VkDescriptorPoolSize> poolCounts{
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
        };

        mDescriptorAllocator.OnInit(poolCounts);
    }

    // Descriptor set for sampling the background cubemap
    mBackgroundDescrptorSetLayout =
        DescriptorSetLayoutBuilder("EnvBackgroundDescriptorLayout")
            .AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT)
            .Build(ctx, mDeletionQueue);

    mBackgroundDescriptorSet =
        mDescriptorAllocator.Allocate(mBackgroundDescrptorSetLayout);

    // Descriptor set for using lighting information:
    mLightingDescriptorSetLayout =
        DescriptorSetLayoutBuilder("EnvLightingDescriptorLayout")
            .AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                        VK_SHADER_STAGE_FRAGMENT_BIT)
            .AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        VK_SHADER_STAGE_FRAGMENT_BIT)
            .AddBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        VK_SHADER_STAGE_FRAGMENT_BIT)
            .AddBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        VK_SHADER_STAGE_FRAGMENT_BIT)
            .Build(ctx, mDeletionQueue);

    mLightingDescriptorSet = mDescriptorAllocator.Allocate(mLightingDescriptorSetLayout);

    // Descriptor set for sampling a texure and saving to image:
    mTexToImgDescriptorSetLayout =
        DescriptorSetLayoutBuilder("EnvTexToImgDescriptorLayout")
            .AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
            .AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        VK_SHADER_STAGE_COMPUTE_BIT)
            .Build(ctx, mDeletionQueue);

    mTexToImgDescriptorSet = mDescriptorAllocator.Allocate(mTexToImgDescriptorSetLayout);

    // Descriptor set for irradiance SH data buffer:
    mIrradianceDescriptorSetLayout =
        DescriptorSetLayoutBuilder("EnvIrradianceDescriptorLayout")
            .AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .Build(ctx, mDeletionQueue);

    mIrradianceDescriptorSet =
        mDescriptorAllocator.Allocate(mIrradianceDescriptorSetLayout);

    // Descriptor set for generation of prefiltered map:
    mPrefilteredDescriptorSetLayout =
        DescriptorSetLayoutBuilder("EnvPrefilteredDescriptorLayout")
            .AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        VK_SHADER_STAGE_COMPUTE_BIT)
            .AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
            .Build(ctx, mDeletionQueue);

    mPrefilteredDescriptorSet =
        mDescriptorAllocator.Allocate(mPrefilteredDescriptorSetLayout);

    // Descriptor set for generating the integration map:
    mIntegrationDescriptorSetLayout =
        DescriptorSetLayoutBuilder("EnvIntegrationDescriptorLayout")
            .AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
            .Build(mCtx, mDeletionQueue);

    mIntegrationDescriptorSet =
        mDescriptorAllocator.Allocate(mIntegrationDescriptorSetLayout);

    // Create cubemap image and view:
    constexpr uint32_t cubeSize = 1024;

    auto cubemapInfo = Image2DInfo{
        .Extent = {cubeSize, cubeSize},
        .Format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .Tiling = VK_IMAGE_TILING_OPTIMAL,
        .Usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                 VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .MipLevels = Image::CalcNumMips(cubeSize, cubeSize),
        .Layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

    mCubemap = MakeTexture::TextureCube(mCtx, "EnvCubemap", cubemapInfo, mDeletionQueue);

    // Create prefiltered map image and view:
    constexpr uint32_t prefilteredSize = 256;
    const uint32_t prefilteredMips = Image::CalcNumMips(prefilteredSize, prefilteredSize);

    auto prefilteredInfo =
        Image2DInfo{.Extent = {prefilteredSize, prefilteredSize},
                    .Format = VK_FORMAT_R32G32B32A32_SFLOAT,
                    .Tiling = VK_IMAGE_TILING_OPTIMAL,
                    .Usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                             VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    .MipLevels = prefilteredMips,
                    .Layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

    mPrefiltered = MakeTexture::TextureCube(mCtx, "EnvPrefilteredMap", prefilteredInfo,
                                            mDeletionQueue);

    // Create single mip views for usage in compute:
    for (uint32_t mip = 0; mip < prefilteredMips; mip++)
    {
        const auto name = "EnvPrefilteredViewMip" + std::to_string(mip);

        auto view = MakeView::ViewCubeSingleMip(mCtx, name, mPrefiltered.Img,
                                                prefilteredInfo.Format,
                                                VK_IMAGE_ASPECT_COLOR_BIT, mip);

        mPrefilteredMipViews.push_back(view);

        mDeletionQueue.push_back(view);
    }

    // Create BRDF integration map:
    constexpr uint32_t integrationSize = 512;

    auto integrationInfo = Image2DInfo{
        .Extent = {integrationSize, integrationSize},
        .Format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .Tiling = VK_IMAGE_TILING_OPTIMAL,
        .Usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .MipLevels = 1,
    };

    mIntegration = MakeTexture::Texture2D(mCtx, "EnvIntegrationMap", integrationInfo,
                                          mDeletionQueue);

    // Create lighting uniform buffer:
    mEnvUBO =
        MakeBuffer::MappedUniform(ctx, "EnvLightUniformBuffer", sizeof(mEnvUBOData));
    mDeletionQueue.push_back(mEnvUBO);

    Buffer::UploadToMapped(mEnvUBO, &mEnvUBOData, sizeof(mEnvUBOData));

    // Shader storage buffer storage for computing irradiance SH:
    {
        const uint32_t groupsPerLine = (cubeSize / mReduceBlock);
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

        VkDeviceSize sizeFirst = mFirstBufferLen * sizeof(SHData);
        VkDeviceSize sizeFinal = 1 * sizeof(SHData);

        auto usageFirst = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        auto usageFinal =
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        mFirstReducionBuffer =
            Buffer::Create(mCtx, "EnvFirstReductionBuffer", sizeFirst, usageFirst);
        mFinalReductionBuffer =
            Buffer::Create(mCtx, "EnvFinalReductionBuffer", sizeFinal, usageFinal);

        mDeletionQueue.push_back(mFirstReducionBuffer);
        mDeletionQueue.push_back(mFinalReductionBuffer);

        // auto GetBufferAddress = [&](VkBuffer buffer) {
        //     VkBufferDeviceAddressInfo deviceAdressInfo{};
        //     deviceAdressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        //     deviceAdressInfo.buffer = buffer;
        //
        //     return vkGetBufferDeviceAddress(mCtx.Device, &deviceAdressInfo);
        // };
        //
        // mFirstBufferAddress = GetBufferAddress(mFirstReducionBuffer.Handle);
        // mFinalBufferAddress = GetBufferAddress(mFinalReductionBuffer.Handle);
    }

    // Update the exposed descriptors:
    DescriptorUpdater(mBackgroundDescriptorSet)
        .WriteImageSampler(0, mCubemap.View, mSampler)
        .Update(mCtx);

    DescriptorUpdater(mLightingDescriptorSet)
        .WriteUniformBuffer(0, mEnvUBO.Handle, sizeof(mEnvUBOData))
        .WriteShaderStorageBuffer(1, mFinalReductionBuffer.Handle,
                                  mFinalReductionBuffer.AllocInfo.size)
        .WriteImageSampler(2, mPrefiltered.View, mSamplerMipped)
        .WriteImageSampler(3, mIntegration.View, mSamplerClamped)
        .Update(mCtx);

    // Update irradiance descriptor
    DescriptorUpdater(mIrradianceDescriptorSet)
        .WriteShaderStorageBuffer(0, mFirstReducionBuffer.Handle,
                                  mFirstReducionBuffer.AllocInfo.size)
        .WriteShaderStorageBuffer(1, mFinalReductionBuffer.Handle,
                                  mFinalReductionBuffer.AllocInfo.size)
        .Update(mCtx);

    // Update integration descriptor
    DescriptorUpdater(mIntegrationDescriptorSet)
        .WriteImageStorage(0, mIntegration.View)
        .Update(mCtx);

    // Build the compute pipelines:
    RebuildPipelines();

    // Generate the integration map (it is cubemap independent):
    GenerateIntegrationMap();
}

EnvironmentHandler::~EnvironmentHandler()
{
    mDescriptorAllocator.DestroyPools();
    mDeletionQueue.flush();
    mPipelineDeletionQueue.flush();
}

void EnvironmentHandler::RebuildPipelines()
{
    mPipelineDeletionQueue.flush();

    mEquiRectToCubePipeline = ComputePipelineBuilder("EnvEquToCubePipeline")
                                  .SetShaderPath("assets/spirv/EquiToCubeComp.spv")
                                  .AddDescriptorSetLayout(mTexToImgDescriptorSetLayout)
                                  .Build(mCtx, mPipelineDeletionQueue);

    mIrradianceSHPipeline = ComputePipelineBuilder("EnvIrradianceSHPipeline")
                                .SetShaderPath("assets/spirv/IrradianceCalcSHComp.spv")
                                .AddDescriptorSetLayout(mBackgroundDescrptorSetLayout)
                                .AddDescriptorSetLayout(mIrradianceDescriptorSetLayout)
                                .SetPushConstantSize(sizeof(IrradianceSHPushConstants))
                                .Build(mCtx, mPipelineDeletionQueue);

    mIrradianceReducePipeline =
        ComputePipelineBuilder("EnvIrradianceReducePipeline")
            .SetShaderPath("assets/spirv/IrradianceReduceComp.spv")
            .AddDescriptorSetLayout(mIrradianceDescriptorSetLayout)
            .SetPushConstantSize(sizeof(ReducePushConstants))
            .Build(mCtx, mPipelineDeletionQueue);

    mPrefilteredGenPipeline = ComputePipelineBuilder("EnvPrefilteredGenPipeline")
                                  .SetShaderPath("assets/spirv/PrefilteredGenComp.spv")
                                  .AddDescriptorSetLayout(mPrefilteredDescriptorSetLayout)
                                  .SetPushConstantSize(sizeof(PrefilteredPushConstants))
                                  .Build(mCtx, mPipelineDeletionQueue);

    mIntegrationGenPipeline = ComputePipelineBuilder("EnvIntegrationPipeline")
                                  .SetShaderPath("assets/spirv/IntegrationGenComp.spv")
                                  .AddDescriptorSetLayout(mIntegrationDescriptorSetLayout)
                                  .Build(mCtx, mPipelineDeletionQueue);
}

void EnvironmentHandler::LoadEnvironment(const Scene &scene)
{
    auto currentHdri = scene.Env.HdriImage;

    const auto maxPrefilteredLod =
        static_cast<float>(std::log2(mPrefiltered.Img.Info.extent.width));

    mEnvUBOData = EnvUBOData{
        .LightDir = scene.Env.LightDir,
        .LightOn = static_cast<int32_t>(scene.Env.DirLightOn),
        .HdriEnabled = currentHdri.has_value(),
        .MaxReflectionLod = maxPrefilteredLod,
    };

    Buffer::UploadToMapped(mEnvUBO, &mEnvUBOData, sizeof(mEnvUBOData));

    if (currentHdri != mLastHdri)
    {
        mLastHdri = currentHdri;

        if (currentHdri.has_value())
        {
            const auto &data = scene.Images.at(*currentHdri);
            const auto format = VK_FORMAT_R32G32B32A32_SFLOAT;

            ConvertEquirectToCubemap(data, format);
            CalculateDiffuseIrradiance();
            GeneratePrefilteredMap();
        }

        else
        {
            ResetToBlack();
        }
    }
}

void EnvironmentHandler::ConvertEquirectToCubemap(const ImageData &data, VkFormat format)
{
    // Load equirectangular environment map:
    auto envMap = TextureLoaders::LoadTexture2D(mCtx, "EnvEnvironmentMap", data, format);

    DescriptorUpdater(mTexToImgDescriptorSet)
        .WriteImageStorage(0, mCubemap.View)
        .WriteImageSampler(1, envMap.View, mSampler)
        .Update(mCtx);

    mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
        // Transition cubemap to use as storage image:
        auto barrierInfo = barrier::ImageLayoutBarrierInfo{
            .Image = mCubemap.Img.Handle,
            .OldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .NewLayout = VK_IMAGE_LAYOUT_GENERAL,
            .SubresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0,
                                 mCubemap.Img.Info.mipLevels, 0, 6},
        };
        barrier::ImageLayoutBarrierCoarse(cmd, barrierInfo);

        // Sample equirectangular map to cubemap
        // using a compute pipeline:
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                          mEquiRectToCubePipeline.Handle);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                mEquiRectToCubePipeline.Layout, 0, 1,
                                &mTexToImgDescriptorSet, 0, nullptr);

        uint32_t localSizeX = 32, localSizeY = 32;

        uint32_t dispCountX = mCubemap.Img.Info.extent.width / localSizeX;
        uint32_t dispCountY = mCubemap.Img.Info.extent.width / localSizeY;

        vkCmdDispatch(cmd, dispCountX, dispCountY, 6);

        // Transition cubemap back to be used as texture:
        barrierInfo.OldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrierInfo.NewLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier::ImageLayoutBarrierCoarse(cmd, barrierInfo);
    });

    // Generate mip levels (to use when generating prefiltered map)
    Image::GenerateMips(mCtx, mCubemap.Img);

    // Clean up the equirectangular map:
    Image::Destroy(mCtx, envMap.Img);
    vkDestroyImageView(mCtx.Device, envMap.View, nullptr);
}

void EnvironmentHandler::CalculateDiffuseIrradiance()
{
    // Do parallel patch-based computation of SH coeficcients
    mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                          mIrradianceSHPipeline.Handle);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                mIrradianceSHPipeline.Layout, 0, 1,
                                &mBackgroundDescriptorSet, 0, nullptr);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                mIrradianceSHPipeline.Layout, 1, 1,
                                &mIrradianceDescriptorSet, 0, nullptr);

        IrradianceSHPushConstants pcData{
            .CubemapRes = mCubemap.Img.Info.extent.width,
            .ReduceBlock = mReduceBlock,
        };

        vkCmdPushConstants(cmd, mIrradianceSHPipeline.Layout, VK_SHADER_STAGE_COMPUTE_BIT,
                           0, sizeof(IrradianceSHPushConstants), &pcData);

        uint32_t localSizeX = 1024;
        uint32_t dispCountX = mFirstBufferLen / localSizeX;

        vkCmdDispatch(cmd, dispCountX, 1, 1);
    });

    // Sum-reduce the resulting array:
    mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                          mIrradianceReducePipeline.Handle);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                mIrradianceReducePipeline.Layout, 0, 1,
                                &mIrradianceDescriptorSet, 0, nullptr);

        ReducePushConstants pcData{
            .BufferSize = mFirstBufferLen,
        };

        vkCmdPushConstants(cmd, mIrradianceSHPipeline.Layout, VK_SHADER_STAGE_COMPUTE_BIT,
                           0, sizeof(ReducePushConstants), &pcData);

        vkCmdDispatch(cmd, 1, 1, 1);
    });
}

void EnvironmentHandler::GeneratePrefilteredMap()
{
    const auto numMips = mPrefiltered.Img.Info.mipLevels;

    // Blit cubemap onto prefiltered map level zero:
    mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
        // Transition cubemap to transfer source:
        auto srcInfo = barrier::ImageLayoutBarrierInfo{
            .Image = mCubemap.Img.Handle,
            .OldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .NewLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .SubresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0,
                                 mCubemap.Img.Info.mipLevels, 0, 6},
        };
        barrier::ImageLayoutBarrierCoarse(cmd, srcInfo);

        // Trasition prefiltered map to transfer destination:
        auto dstInfo = barrier::ImageLayoutBarrierInfo{
            .Image = mPrefiltered.Img.Handle,
            .OldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .NewLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .SubresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6},
        };
        barrier::ImageLayoutBarrierCoarse(cmd, dstInfo);

        // Issue the blit command:
        vkutils::BlitImageZeroMip(cmd, mCubemap.Img, mPrefiltered.Img);

        // Transition cubemap to be used as a texture:
        srcInfo.OldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        srcInfo.NewLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier::ImageLayoutBarrierCoarse(cmd, srcInfo);

        // Transition whole prefiltered map to use as storage image:
        auto barrierInfo = barrier::ImageLayoutBarrierInfo{
            .Image = mPrefiltered.Img.Handle,
            .OldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .NewLayout = VK_IMAGE_LAYOUT_GENERAL,
            .SubresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, numMips, 0, 6},
        };
        barrier::ImageLayoutBarrierCoarse(cmd, barrierInfo);
    });

    // Generate higher mips by integrating cubemap
    // over increasingly larger lobes.
    {
        uint32_t resX = mPrefiltered.Img.Info.extent.width / 2;
        uint32_t resY = mPrefiltered.Img.Info.extent.height / 2;

        for (uint32_t mip = 1; mip < numMips; mip++)
        {
            const float roughness =
                static_cast<float>(mip) / static_cast<float>(numMips - 1);

            PrefilteredPushConstants pcData{
                .CubeResolution = mCubemap.Img.Info.extent.width,
                .MipLevel = mip,
                .Roughness = roughness,
            };

            // Update prefiltered descriptor to access appropriate mip level
            DescriptorUpdater(mPrefilteredDescriptorSet)
                .WriteImageSampler(0, mCubemap.View, mSamplerMipped)
                .WriteImageStorage(1, mPrefilteredMipViews[mip])
                .Update(mCtx);

            mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                  mPrefilteredGenPipeline.Handle);

                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                        mPrefilteredGenPipeline.Layout, 0, 1,
                                        &mPrefilteredDescriptorSet, 0, nullptr);

                vkCmdPushConstants(cmd, mPrefilteredGenPipeline.Layout,
                                   VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                   sizeof(PrefilteredPushConstants), &pcData);

                vkCmdDispatch(cmd, resX, resY, 6);
            });

            resX = resX / 2;
            resY = resY / 2;
        }
    }

    // Transition prefiltered map to be used as a texture:
    mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
        auto dstInfo = barrier::ImageLayoutBarrierInfo{
            .Image = mPrefiltered.Img.Handle,
            .OldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .NewLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .SubresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, numMips, 0, 6},
        };

        barrier::ImageLayoutBarrierCoarse(cmd, dstInfo);
    });
}

void EnvironmentHandler::GenerateIntegrationMap()
{
    mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
        auto barrierInfo = barrier::ImageLayoutBarrierInfo{
            .Image = mIntegration.Img.Handle,
            .OldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .NewLayout = VK_IMAGE_LAYOUT_GENERAL,
            .SubresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        };
        barrier::ImageLayoutBarrierCoarse(cmd, barrierInfo);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                          mIntegrationGenPipeline.Handle);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                mIntegrationGenPipeline.Layout, 0, 1,
                                &mIntegrationDescriptorSet, 0, nullptr);

        uint32_t localSizeX = 32, localSizeY = 32;

        uint32_t dispCountX = mIntegration.Img.Info.extent.width / localSizeX;
        uint32_t dispCountY = mIntegration.Img.Info.extent.width / localSizeY;

        vkCmdDispatch(cmd, dispCountX, dispCountY, 1);

        barrierInfo.OldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrierInfo.NewLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier::ImageLayoutBarrierCoarse(cmd, barrierInfo);
    });
}

void EnvironmentHandler::ResetToBlack()
{
    mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
        auto clearImage = [&](Image &img) {
            VkClearColorValue black = {{0.0f, 0.0f, 0.0f, 1.0f}};

            auto range = VkImageSubresourceRange{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = img.Info.mipLevels,
                .baseArrayLayer = 0,
                .layerCount = img.Info.arrayLayers,
            };

            auto barrierInfo = barrier::ImageLayoutBarrierInfo{
                .Image = img.Handle,
                .OldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .NewLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .SubresourceRange = range,
            };
            barrier::ImageLayoutBarrierCoarse(cmd, barrierInfo);

            vkCmdClearColorImage(cmd, img.Handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                 &black, 1, &range);

            barrierInfo.OldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrierInfo.NewLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier::ImageLayoutBarrierCoarse(cmd, barrierInfo);
        };

        // Clear cubemap and prefiltered map to pure black:
        clearImage(mCubemap.Img);
        clearImage(mPrefiltered.Img);

        // Set all SH coefficients to 0:
        vkCmdFillBuffer(cmd, mFinalReductionBuffer.Handle, 0,
                        mFinalReductionBuffer.AllocInfo.size,
                        std::bit_cast<uint32_t>(0.0f));
    });
}