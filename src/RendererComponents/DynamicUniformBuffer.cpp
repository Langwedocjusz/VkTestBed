#include "DynamicUniformBuffer.h"
#include "Pch.h"

#include "BufferUtils.h"
#include "Descriptor.h"
#include "Vassert.h"

DynamicUniformBuffer::DynamicUniformBuffer(VulkanContext &ctx, FrameInfo &frame,
                                           VkDeviceSize bufferSize)
    : mBufferSize(bufferSize), mCtx(ctx), mFrame(frame), mDeletionQueue(ctx)
{
    // Create Uniform Buffers:
    mUniformBuffers.resize(mCtx.Swapchain.image_count);

    for (auto &uniformBuffer : mUniformBuffers)
    {
        uniformBuffer =
            MakeBuffer::MappedUniform(mCtx, "CameraUniformBuffer", bufferSize);
        mDeletionQueue.push_back(uniformBuffer);
    }
}

void DynamicUniformBuffer::Initialize(std::string_view   debugName,
                                      VkShaderStageFlags stageFlags)
{
    vassert(!mInitialized, "Already initialized!");
    mInitialized = true;

    uint32_t numBuffers = mUniformBuffers.size();

    // Create descriptor set layout:
    mDescriptorSetLayout = DescriptorSetLayoutBuilder(debugName)
                               .AddUniformBuffer(0, stageFlags)
                               .Build(mCtx, mDeletionQueue);

    // Initialize descriptor pool:

    // clang-format off
    std::array<VkDescriptorPoolSize, 1> poolCounts{{
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, numBuffers},
    }};
    // clang-format on

    mDescriptorPool = Descriptor::InitPool(mCtx, numBuffers, poolCounts, mDeletionQueue);

    // Allocate descriptor sets:
    std::vector<VkDescriptorSetLayout> layouts(numBuffers, mDescriptorSetLayout);

    mDescriptorSets = Descriptor::Allocate(mCtx, mDescriptorPool, layouts);

    // Update descriptor sets:
    for (size_t i = 0; i < mDescriptorSets.size(); i++)
    {
        DescriptorUpdater(mDescriptorSets[i])
            .WriteUniformBuffer(0, mUniformBuffers[i].Handle, mBufferSize)
            .Update(mCtx);
    }
}

void DynamicUniformBuffer::UpdateData(void *data, VkDeviceSize size)
{
    vassert(mInitialized, "Not initialized yet!");

    auto &uniformBuffer = mUniformBuffers[mFrame.ImageIndex];
    Buffer::UploadToMapped(uniformBuffer, data, size);
}

VkDescriptorSetLayout DynamicUniformBuffer::DescriptorSetLayout() const
{
    vassert(mInitialized, "Not initialized yet!");

    return mDescriptorSetLayout;
}

VkDescriptorSet DynamicUniformBuffer::DescriptorSet() const
{
    vassert(mInitialized, "Not initialized yet!");

    return mDescriptorSets[mFrame.ImageIndex];
}
