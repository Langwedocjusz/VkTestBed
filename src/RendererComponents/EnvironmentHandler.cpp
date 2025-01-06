#include "EnvironmentHandler.h"

#include "Sampler.h"
#include "Shader.h"
#include "Barrier.h"
#include "Utils.h"
#include "ImageLoaders.h"

EnvironmentHandler::EnvironmentHandler(VulkanContext& ctx, FrameInfo &info, RenderContext::Queues &queues)
    : mCtx(ctx), mFrame(info), mQueues(queues),
    mDescriptorAllocator(ctx), mDeletionQueue(ctx), mPipelineDeletionQueue(ctx)
{
    // Create the texture sampler:
    mSampler = SamplerBuilder()
                     .SetMagFilter(VK_FILTER_LINEAR)
                     .SetMinFilter(VK_FILTER_LINEAR)
                     .SetAddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT)
                     .Build(mCtx);
    mDeletionQueue.push_back(mSampler);

    // Descriptor layout for generating the cubemap:
    mCubeGenDescriptorSetLayout =
        DescriptorSetLayoutBuilder()
            .AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
            .AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        VK_SHADER_STAGE_COMPUTE_BIT)
            .Build(ctx);

    mDeletionQueue.push_back(mCubeGenDescriptorSetLayout);

    // Descriptor layout for sampling the cubemap and accessing lighting info:
    mEnvironmentDescriptorSetLayout =
        DescriptorSetLayoutBuilder()
            .AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        VK_SHADER_STAGE_FRAGMENT_BIT)
            .AddBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                        VK_SHADER_STAGE_FRAGMENT_BIT)
            .Build(ctx);

    mDeletionQueue.push_back(mEnvironmentDescriptorSetLayout);

    // Initialize main descriptor allocator:
    {
        std::vector<VkDescriptorPoolSize> poolCounts{
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}};

        mDescriptorAllocator.OnInit(poolCounts);
    }

    // Allocate descriptor sets:
    mEnvironmentDescriptorSet =
        mDescriptorAllocator.Allocate(mEnvironmentDescriptorSetLayout);

    mCubeGenDescriptorSet =
        mDescriptorAllocator.Allocate(mCubeGenDescriptorSetLayout);

    // Create cubemap image and view:
    constexpr uint32_t cubeSize = 1024;

    auto cubemapInfo = ImageInfo{
        .Extent = {cubeSize, cubeSize, 1},
        .Format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .Tiling = VK_IMAGE_TILING_OPTIMAL,
        .Usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    };

    mCubemap = Image::CreateCubemap(mCtx, cubemapInfo);
    mDeletionQueue.push_back(mCubemap);

    mCubemapView = Image::CreateViewCube(mCtx, mCubemap, cubemapInfo.Format,
                                         VK_IMAGE_ASPECT_COLOR_BIT);
    mDeletionQueue.push_back(mCubemapView);

    // Uniform buffer for other environment data:
    mEnvUBO = Buffer::CreateMappedUniformBuffer(ctx, sizeof(mEnvUBOData));
    mDeletionQueue.push_back(mEnvUBO);

    Buffer::UploadToMappedBuffer(mEnvUBO, &mEnvUBOData, sizeof(mEnvUBOData));

    // Update the environment descriptor:
    DescriptorUpdater(mEnvironmentDescriptorSet)
        .WriteImageSampler(0, mCubemapView, mSampler)
        .WriteUniformBuffer(1, mEnvUBO.Handle, sizeof(mEnvUBOData))
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
                                  .AddDescriptorSetLayout(mCubeGenDescriptorSetLayout)
                                  .SetPushConstantSize(sizeof(CubeGenPCData))
                                  .Build(mCtx);

    mPipelineDeletionQueue.push_back(mEquiRectToCubePipeline.Handle);
    mPipelineDeletionQueue.push_back(mEquiRectToCubePipeline.Layout);
}

void EnvironmentHandler::LoadEnvironment(Scene& scene)
{
    auto &path = scene.Env.HdriPath;

    mEnvUBOData.HdriEnabled = path.has_value();
    mEnvUBOData.LightDir = glm::vec4(scene.Env.LightDir, float(scene.Env.DirLightOn));

    Buffer::UploadToMappedBuffer(mEnvUBO, &mEnvUBOData, sizeof(mEnvUBOData));

    bool updateCubemap = path.has_value() && (path != mLastHdri);

    if (updateCubemap)
    {
        mLastHdri = *path;

        auto &pool = mFrame.CurrentPool();

        // Load equirectangular environment map:
        auto envMap = ImageLoaders::LoadHDRI(mCtx, mQueues.Graphics, pool, *path);

        auto envView = Image::CreateView2D(mCtx, envMap, envMap.Info.Format,
                                           VK_IMAGE_ASPECT_COLOR_BIT);

        DescriptorUpdater(mCubeGenDescriptorSet)
            .WriteImageStorage(0, mCubemapView)
            .WriteImageSampler(1, envView, mSampler)
            .Update(mCtx);

        // Sample equirectangular map to cubemap
        // using a compute pipeline:
        {
            utils::ScopedCommand cmd(mCtx, mQueues.Graphics, pool);

            auto barrierInfo = barrier::ImageLayoutBarrierInfo{
                .Image = mCubemap.Handle,
                .OldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .NewLayout = VK_IMAGE_LAYOUT_GENERAL,
                .SubresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6},
            };
            barrier::ImageLayoutBarrierCoarse(cmd.Buffer, barrierInfo);

            vkCmdBindPipeline(cmd.Buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                              mEquiRectToCubePipeline.Handle);

            vkCmdBindDescriptorSets(cmd.Buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                    mEquiRectToCubePipeline.Layout, 0, 1,
                                    &mCubeGenDescriptorSet, 0, nullptr);

            for (int32_t side = 0; side < 6; side++)
            {
                auto pcData = CubeGenPCData{};
                pcData.SideId = side;

                vkCmdPushConstants(cmd.Buffer, mEquiRectToCubePipeline.Layout,
                                   VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pcData),
                                   &pcData);

                uint32_t localSizeX = 32, localSizeY = 32;

                uint32_t dispCountX = mCubemap.Info.Extent.width / localSizeX;
                uint32_t dispCountY = mCubemap.Info.Extent.width / localSizeY;

                vkCmdDispatch(cmd.Buffer, dispCountX, dispCountY, 1);
            }

            barrierInfo.OldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrierInfo.NewLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier::ImageLayoutBarrierCoarse(cmd.Buffer, barrierInfo);
        }

        // Clean up the equirectangular map:
        Image::DestroyImage(mCtx, envMap);
        vkDestroyImageView(mCtx.Device, envView, nullptr);
    }
}