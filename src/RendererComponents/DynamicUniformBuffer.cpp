#include "DynamicUniformBuffer.h"

#include "Pch.h"

#include "BufferUtils.h"
#include "Descriptor.h"
#include "Vassert.h"

DynamicUniformBuffer::DynamicUniformBuffer(VulkanContext &ctx, FrameInfo &frame)
    : mCtx(ctx), mFrame(frame), mDeletionQueue(ctx)
{
}

DynamicUniformBuffer::~DynamicUniformBuffer()
{
    mDeletionQueue.flush();
}

void DynamicUniformBuffer::OnInit(std::string_view   debugName,
                                  VkShaderStageFlags stageFlags, VkDeviceSize bufferSize)
{
    vassert(!mInitialized, "Already initialized!");

    mInitialized = true;

    // Create descriptor sets:
    //  Descriptor layout
    mDescriptorSetLayout =
        DescriptorSetLayoutBuilder(debugName)
            .AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, stageFlags)
            .Build(mCtx, mDeletionQueue);

    // Descriptor pool
    uint32_t maxSets = mCtx.Swapchain.image_count;

    std::vector<VkDescriptorPoolSize> poolCounts{
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, maxSets},
    };

    mDescriptorPool = Descriptor::InitPool(mCtx, maxSets, poolCounts);
    mDeletionQueue.push_back(mDescriptorPool);

    // Descriptor sets allocation
    std::vector<VkDescriptorSetLayout> layouts(maxSets, mDescriptorSetLayout);

    mDescriptorSets = Descriptor::Allocate(mCtx, mDescriptorPool, layouts);

    // Create Uniform Buffers:
    mUniformBuffers.resize(maxSets);

    for (auto &uniformBuffer : mUniformBuffers)
    {
        uniformBuffer =
            MakeBuffer::MappedUniform(mCtx, "CameraUniformBuffer", bufferSize);
        mDeletionQueue.push_back(uniformBuffer);
    }

    // Update descriptor sets:
    for (size_t i = 0; i < mDescriptorSets.size(); i++)
    {
        DescriptorUpdater(mDescriptorSets[i])
            .WriteUniformBuffer(0, mUniformBuffers[i].Handle, bufferSize)
            .Update(mCtx);
    }
}

void DynamicUniformBuffer::UpdateData(void *data, VkDeviceSize size)
{
    vassert(mInitialized, "Not initialized yet!");

    auto &uniformBuffer = mUniformBuffers[mFrame.ImageIndex];
    Buffer::UploadToMapped(uniformBuffer, data, size);
}