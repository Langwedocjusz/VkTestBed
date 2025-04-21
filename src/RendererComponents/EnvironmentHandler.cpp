#include "EnvironmentHandler.h"

#include "Barrier.h"
#include "Descriptor.h"
#include "ImageLoaders.h"
#include "Sampler.h"
#include "Shader.h"
#include "VkUtils.h"
#include <vulkan/vulkan_core.h>

EnvironmentHandler::EnvironmentHandler(VulkanContext &ctx, FrameInfo &info,
                                       RenderContext::Queues &queues)
    : mCtx(ctx), mFrame(info), mQueues(queues), mDescriptorAllocator(ctx),
      mDeletionQueue(ctx), mPipelineDeletionQueue(ctx)
{
    // Create the texture sampler:
    mSampler = SamplerBuilder()
                   .SetMagFilter(VK_FILTER_LINEAR)
                   .SetMinFilter(VK_FILTER_LINEAR)
                   .SetAddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT)
                   .Build(mCtx);
    mDeletionQueue.push_back(mSampler);

    // Initialize main descriptor allocator:
    {
        std::vector<VkDescriptorPoolSize> poolCounts{
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
        };

        mDescriptorAllocator.OnInit(poolCounts);
    }

    // Descriptor set for sampling the background cubemap
    mBackgroundDescrptorSetLayout =
        DescriptorSetLayoutBuilder()
            .AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT)
            .Build(ctx);

    mDeletionQueue.push_back(mBackgroundDescrptorSetLayout);

    mBackgroundDescriptorSet =
        mDescriptorAllocator.Allocate(mBackgroundDescrptorSetLayout);

    // Descriptor set for using lighting information:
    mLightingDescriptorSetLayout = DescriptorSetLayoutBuilder()
                                       .AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                   VK_SHADER_STAGE_FRAGMENT_BIT)
                                       .AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                   VK_SHADER_STAGE_FRAGMENT_BIT)
                                       .Build(ctx);

    mDeletionQueue.push_back(mLightingDescriptorSetLayout);

    mLightingDescriptorSet = mDescriptorAllocator.Allocate(mLightingDescriptorSetLayout);

    // Descriptor set for sampling a texure and saving to image:
    mTexToImgDescriptorSetLayout =
        DescriptorSetLayoutBuilder()
            .AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
            .AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        VK_SHADER_STAGE_COMPUTE_BIT)
            .Build(ctx);

    mDeletionQueue.push_back(mTexToImgDescriptorSetLayout);

    mTexToImgDescriptorSet = mDescriptorAllocator.Allocate(mTexToImgDescriptorSetLayout);

    // Descriptor set for irradiance SH data buffer:
    mIrradianceDescriptorSetLayout =
        DescriptorSetLayoutBuilder()
            .AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .Build(ctx);

    mDeletionQueue.push_back(mIrradianceDescriptorSetLayout);

    mIrradianceDescriptorSet =
        mDescriptorAllocator.Allocate(mIrradianceDescriptorSetLayout);

    // Create cubemap image and view:
    constexpr uint32_t cubeSize = 1024;

    auto cubemapInfo = ImageInfo{
        .Extent = {cubeSize, cubeSize, 1},
        .Format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .Tiling = VK_IMAGE_TILING_OPTIMAL,
        .Usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .MipLevels = 1,
    };

    mCubemap.Img = Image::CreateCubemap(mCtx, cubemapInfo);
    mCubemap.View = Image::CreateViewCube(mCtx, mCubemap.Img, cubemapInfo.Format,
                                          VK_IMAGE_ASPECT_COLOR_BIT);

    mDeletionQueue.push_back(mCubemap.Img);
    mDeletionQueue.push_back(mCubemap.View);

    // Immidiately transition to shader read layout:
    {
        auto &pool = mFrame.CurrentPool();
        vkutils::ScopedCommand cmd(mCtx, mQueues.Graphics, pool);

        auto barrierInfo = barrier::ImageLayoutBarrierInfo{
            .Image = mCubemap.Img.Handle,
            .OldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .NewLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .SubresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6},
        };

        barrier::ImageLayoutBarrierCoarse(cmd.Buffer, barrierInfo);
    }

    // Create lighting uniform buffer:
    mEnvUBO = Buffer::CreateMappedUniformBuffer(ctx, sizeof(mEnvUBOData));
    mDeletionQueue.push_back(mEnvUBO);

    Buffer::UploadToMappedBuffer(mEnvUBO, &mEnvUBOData, sizeof(mEnvUBOData));

    // Shader storage buffer storage for computing irradiance SH:
    {
        auto usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

        const uint32_t groupsPerLine = (cubeSize / mReduceBlock);
        const uint32_t groupsPerSide = groupsPerLine * groupsPerLine;

        mFirstBufferLen = 6 * groupsPerSide;
        // const uint32_t numGroups2 = numGroups1 / (localGroupSize * localGroupSize);

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

        mFirstReducionBuffer = Buffer::CreateBuffer(mCtx, sizeFirst, usage);
        mFinalReductionBuffer = Buffer::CreateBuffer(mCtx, sizeFinal, usage);

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
        .Update(mCtx);

    // Update irradiance descriptor
    DescriptorUpdater(mIrradianceDescriptorSet)
        .WriteShaderStorageBuffer(0, mFirstReducionBuffer.Handle,
                                  mFirstReducionBuffer.AllocInfo.size)
        .WriteShaderStorageBuffer(1, mFinalReductionBuffer.Handle,
                                  mFinalReductionBuffer.AllocInfo.size)
        .Update(mCtx);

    // Build the compute pipeline:
    RebuildPipelines();
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

    auto equiToCubeShaderStages =
        ShaderBuilder().SetComputePath("assets/spirv/EquiToCubeComp.spv").Build(mCtx);

    mEquiRectToCubePipeline = ComputePipelineBuilder()
                                  .SetShaderStage(equiToCubeShaderStages[0])
                                  .AddDescriptorSetLayout(mTexToImgDescriptorSetLayout)
                                  .Build(mCtx);

    mPipelineDeletionQueue.push_back(mEquiRectToCubePipeline.Handle);
    mPipelineDeletionQueue.push_back(mEquiRectToCubePipeline.Layout);

    auto irrSHShaderStages = ShaderBuilder()
                                 .SetComputePath("assets/spirv/IrradianceCalcSHComp.spv")
                                 .Build(mCtx);

    mIrradianceSHPipeline = ComputePipelineBuilder()
                                .SetShaderStage(irrSHShaderStages[0])
                                .AddDescriptorSetLayout(mBackgroundDescrptorSetLayout)
                                .AddDescriptorSetLayout(mIrradianceDescriptorSetLayout)
                                .SetPushConstantSize(sizeof(IrradianceSHPushConstants))
                                .Build(mCtx);

    mPipelineDeletionQueue.push_back(mIrradianceSHPipeline.Handle);
    mPipelineDeletionQueue.push_back(mIrradianceSHPipeline.Layout);

    auto irrReduceShaderStages =
        ShaderBuilder()
            .SetComputePath("assets/spirv/IrradianceReduceComp.spv")
            .Build(mCtx);

    mIrradianceReducePipeline =
        ComputePipelineBuilder()
            .SetShaderStage(irrReduceShaderStages[0])
            .AddDescriptorSetLayout(mIrradianceDescriptorSetLayout)
            .SetPushConstantSize(sizeof(ReducePushConstants))
            .Build(mCtx);

    mPipelineDeletionQueue.push_back(mIrradianceReducePipeline.Handle);
    mPipelineDeletionQueue.push_back(mIrradianceReducePipeline.Layout);
}

void EnvironmentHandler::LoadEnvironment(const Scene &scene)
{
    auto key = scene.Env.HdriImage;

    mEnvUBOData.HdriEnabled = key.has_value();
    mEnvUBOData.LightDir = glm::vec4(scene.Env.LightDir, float(scene.Env.DirLightOn));

    Buffer::UploadToMappedBuffer(mEnvUBO, &mEnvUBOData, sizeof(mEnvUBOData));

    bool updateCubemap = key.has_value() && (key != mLastHdri);

    if (updateCubemap)
    {
        mLastHdri = key;

        const auto &data = scene.Images.at(*key);
        const auto format = VK_FORMAT_R32G32B32A32_SFLOAT;

        ConvertEquirectToCubemap(data, format);
        // CalculateDiffuseIrradiance();
    }

    CalculateDiffuseIrradiance();
}

void EnvironmentHandler::ConvertEquirectToCubemap(const ImageData &data, VkFormat format)
{
    auto &pool = mFrame.CurrentPool();

    // Load equirectangular environment map:
    auto envMap = ImageLoaders::LoadImage2D(mCtx, mQueues.Graphics, pool, data, format);

    auto envView =
        Image::CreateView2D(mCtx, envMap, envMap.Info.Format, VK_IMAGE_ASPECT_COLOR_BIT);

    DescriptorUpdater(mTexToImgDescriptorSet)
        .WriteImageStorage(0, mCubemap.View)
        .WriteImageSampler(1, envView, mSampler)
        .Update(mCtx);

    {
        vkutils::ScopedCommand cmd(mCtx, mQueues.Graphics, pool);

        // Transition cubemap to use as storage image:
        auto barrierInfo = barrier::ImageLayoutBarrierInfo{
            .Image = mCubemap.Img.Handle,
            .OldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .NewLayout = VK_IMAGE_LAYOUT_GENERAL,
            .SubresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6},
        };
        barrier::ImageLayoutBarrierCoarse(cmd.Buffer, barrierInfo);

        // Sample equirectangular map to cubemap
        // using a compute pipeline:
        vkCmdBindPipeline(cmd.Buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                          mEquiRectToCubePipeline.Handle);

        vkCmdBindDescriptorSets(cmd.Buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                mEquiRectToCubePipeline.Layout, 0, 1,
                                &mTexToImgDescriptorSet, 0, nullptr);

        uint32_t localSizeX = 32, localSizeY = 32;

        uint32_t dispCountX = mCubemap.Img.Info.Extent.width / localSizeX;
        uint32_t dispCountY = mCubemap.Img.Info.Extent.width / localSizeY;

        vkCmdDispatch(cmd.Buffer, dispCountX, dispCountY, 6);

        // Transition cubemap back to be used as texture:
        barrierInfo.OldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrierInfo.NewLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier::ImageLayoutBarrierCoarse(cmd.Buffer, barrierInfo);
    }

    // Clean up the equirectangular map:
    Image::DestroyImage(mCtx, envMap);
    vkDestroyImageView(mCtx.Device, envView, nullptr);
}

void EnvironmentHandler::CalculateDiffuseIrradiance()
{
    auto &pool = mFrame.CurrentPool();

    // Do parallel patch-based computation of SH coeficcients
    {
        vkutils::ScopedCommand cmd(mCtx, mQueues.Graphics, pool);

        vkCmdBindPipeline(cmd.Buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                          mIrradianceSHPipeline.Handle);

        vkCmdBindDescriptorSets(cmd.Buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                mIrradianceSHPipeline.Layout, 0, 1,
                                &mBackgroundDescriptorSet, 0, nullptr);

        vkCmdBindDescriptorSets(cmd.Buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                mIrradianceSHPipeline.Layout, 1, 1,
                                &mIrradianceDescriptorSet, 0, nullptr);

        IrradianceSHPushConstants pcData{
            .CubemapRes = mCubemap.Img.Info.Extent.width,
            .ReduceBlock = mReduceBlock,
        };

        vkCmdPushConstants(cmd.Buffer, mIrradianceSHPipeline.Layout,
                           VK_SHADER_STAGE_COMPUTE_BIT, 0,
                           sizeof(IrradianceSHPushConstants), &pcData);

        uint32_t localSizeX = 1024;
        uint32_t dispCountX = mFirstBufferLen / localSizeX;

        vkCmdDispatch(cmd.Buffer, dispCountX, 1, 1);
    }

    // Sum-reduce the resulting array:
    {
        vkutils::ScopedCommand cmd(mCtx, mQueues.Graphics, pool);

        vkCmdBindPipeline(cmd.Buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                          mIrradianceReducePipeline.Handle);

        vkCmdBindDescriptorSets(cmd.Buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                mIrradianceReducePipeline.Layout, 0, 1,
                                &mIrradianceDescriptorSet, 0, nullptr);

        ReducePushConstants pcData{
            .BufferSize = mFirstBufferLen,
        };

        vkCmdPushConstants(cmd.Buffer, mIrradianceSHPipeline.Layout,
                           VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ReducePushConstants),
                           &pcData);

        vkCmdDispatch(cmd.Buffer, 1, 1, 1);
    }
}