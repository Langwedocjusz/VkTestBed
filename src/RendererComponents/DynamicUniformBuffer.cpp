#include "DynamicUniformBuffer.h"
#include "Pch.h"

#include "Descriptor.h"
#include "MakeBuffer.h"
#include "Vassert.h"

#include <ranges>

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

void DynamicUniformBuffer::UpdateData(void *data, VkDeviceSize size)
{
    auto &uniformBuffer = mUniformBuffers[mFrame.ImageIndex];
    Buffer::UploadToMapped(uniformBuffer, data, size);
}

DynamicUniformBuffer::HandlesAndSizes DynamicUniformBuffer::GetBufferHandlesAndSizes()
    const
{
    std::vector<VkBuffer> handles;
    handles.resize(mUniformBuffers.size());

    for (size_t idx = 0; idx < mUniformBuffers.size(); idx++)
    {
        handles[idx] = mUniformBuffers[idx].Handle;
    }

    std::vector<VkDeviceSize> sizes(mUniformBuffers.size(), mBufferSize);

    return {handles, sizes};
}

DynamicDescriptorSet::DynamicDescriptorSet(VulkanContext &ctx, FrameInfo &frame)
    : mCtx(ctx), mFrame(frame), mDeletionQueue(ctx)
{
}

void DynamicDescriptorSet::Initialize(VkDescriptorSetLayout           layout,
                                      std::span<VkDescriptorPoolSize> poolCounts)
{
    vassert(!mInitialized, "Already initialized!");
    mInitialized = true;

    // Initialize descriptor pool:
    uint32_t numDescriptors = mCtx.Swapchain.image_count;

    mDescriptorPool =
        Descriptor::InitPool(mCtx, numDescriptors, poolCounts, mDeletionQueue);

    // Allocate descriptor sets:
    mDescriptorSetLayout = layout;
    std::vector<VkDescriptorSetLayout> layouts(numDescriptors, mDescriptorSetLayout);

    mDescriptorSets = Descriptor::Allocate(mCtx, mDescriptorPool, layouts);
}

void DynamicDescriptorSet::BeginUpdate()
{
    vassert(mInitialized, "Not initialized yet!");
    vassert(mUpdaters.empty(), "EndUpdate was not called since last update!");

    for (auto &descriptorSet : mDescriptorSets)
    {
        mUpdaters.emplace_back(descriptorSet);
    }
}

void DynamicDescriptorSet::EndUpdate()
{
    vassert(mInitialized, "Not initialized yet!");
    vassert(!mUpdaters.empty(), "BeginUpdate was not called since last update!");

    for (auto &updater : mUpdaters)
    {
        updater.Update(mCtx);
    }

    mUpdaters.clear();
}

void DynamicDescriptorSet::WriteUniformBuffer(uint32_t binding, VkBuffer buffer,
                                              VkDeviceSize size)
{
    vassert(mInitialized, "Not initialized yet!");
    vassert(!mUpdaters.empty(), "BeginUpdate was not called since last update!");

    for (auto &updater : mUpdaters)
    {
        updater.WriteUniformBuffer(binding, buffer, size);
    }
}

void DynamicDescriptorSet::WriteUniformBuffer(uint32_t                binding,
                                              std::span<VkBuffer>     buffers,
                                              std::span<VkDeviceSize> sizes)
{
    vassert(mInitialized, "Not initialized yet!");
    vassert(!mUpdaters.empty(), "BeginUpdate was not called since last update!");
    vassert(mUpdaters.size() == buffers.size(), "Invalid buffer count.");
    vassert(mUpdaters.size() == sizes.size(), "Invalid buffer sizes count.");

    using namespace std::views;

    for (auto [idx, updater] : enumerate(mUpdaters))
    {
        updater.WriteUniformBuffer(binding, buffers[idx], sizes[idx]);
    }
}

void DynamicDescriptorSet::WriteCombinedSampler(uint32_t binding, VkImageView imageView,
                                                VkSampler sampler)
{
    vassert(mInitialized, "Not initialized yet!");
    vassert(!mUpdaters.empty(), "BeginUpdate was not called since last update!");

    for (auto &updater : mUpdaters)
    {
        updater.WriteCombinedSampler(binding, imageView, sampler);
    }
}

void DynamicDescriptorSet::WriteCombinedSampler(uint32_t               binding,
                                                std::span<VkImageView> imageViews,
                                                std::span<VkSampler>   samplers)
{
    vassert(mInitialized, "Not initialized yet!");
    vassert(!mUpdaters.empty(), "BeginUpdate was not called since last update!");
    vassert(mUpdaters.size() == imageViews.size(), "Invalid image views count.");
    vassert(mUpdaters.size() == samplers.size(), "Invalid samplers count.");

    using namespace std::views;

    for (auto [idx, updater] : enumerate(mUpdaters))
    {
        updater.WriteCombinedSampler(binding, imageViews[idx], samplers[idx]);
    }
}

VkDescriptorSetLayout DynamicDescriptorSet::DescriptorSetLayout() const
{
    vassert(mInitialized, "Not initialized yet!");

    return mDescriptorSetLayout;
}

VkDescriptorSet DynamicDescriptorSet::DescriptorSet() const
{
    vassert(mInitialized, "Not initialized yet!");

    return mDescriptorSets[mFrame.ImageIndex];
}
