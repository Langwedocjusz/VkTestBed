#include "DynamicUniformBuffer.h"
#include "Pch.h"

#include "Descriptor.h"
#include "Vassert.h"

#include <ranges>

DynamicUniformBuffer::DynamicUniformBuffer(VulkanContext &ctx, FrameInfo &frame, VkDeviceSize bufferSize)
    : mCtx(ctx)
    , mFrame(frame)
    , mDeletionQueue(ctx)
{
    // Create Uniform Buffers:
    const auto numBuffers = mCtx.Swapchain->Images.size();

    mUniformBuffers.reserve(numBuffers);

    for (size_t i = 0; i < numBuffers; i++)
    {
        Buffer::ThinInfo uboInfo{
            .Name = "DynamicUniformBuffer",
            .Size = bufferSize,
        };

        mUniformBuffers.emplace_back(Buffer::MakeMappedUniform(mCtx, uboInfo));
    }
}

void DynamicUniformBuffer::UpdateData(void *data, VkDeviceSize size)
{
    auto &uniformBuffer = mUniformBuffers[mFrame.ImageIndex];
    uniformBuffer.UploadMapped(data, size);
}

std::span<Buffer> DynamicUniformBuffer::GetBuffers()
{
    return {mUniformBuffers};
}

DynamicDescriptorSet::DynamicDescriptorSet(VulkanContext &ctx, FrameInfo &frame)
    : mCtx(ctx)
    , mFrame(frame)
    , mDeletionQueue(ctx)
{
}

void DynamicDescriptorSet::Initialize(VkDescriptorSetLayout layout, Descriptor::PoolCounts counts)
{
    vassert(!mInitialized, "Already initialized!");
    mInitialized = true;

    mDescriptorSetLayout = layout;

    // Initialize descriptor pool:
    auto numDescriptors = static_cast<uint32_t>(mCtx.Swapchain->Images.size());

    auto totalCounts = Descriptor::PoolCounts{
        .DescriptorSet        = numDescriptors,
        .StorageImage         = numDescriptors * counts.StorageImage,
        .CombinedImageSampler = numDescriptors * counts.CombinedImageSampler,
        .UniformBuffer        = numDescriptors * counts.UniformBuffer,
        .StorageBuffer        = numDescriptors * counts.StorageBuffer,
    };

    mDescriptorPool = Descriptor::InitPool(mCtx, totalCounts, mDeletionQueue);

    // Allocate descriptor sets:
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

void DynamicDescriptorSet::WriteUniformBuffer(uint32_t binding, Buffer &buffer)
{
    vassert(mInitialized, "Not initialized yet!");
    vassert(!mUpdaters.empty(), "BeginUpdate was not called since last update!");

    for (auto &updater : mUpdaters)
    {
        updater.WriteUniformBuffer(binding, buffer);
    }
}

void DynamicDescriptorSet::WriteUniformBuffer(
    uint32_t binding, std::span<Buffer> buffers
)
{
    vassert(mInitialized, "Not initialized yet!");
    vassert(!mUpdaters.empty(), "BeginUpdate was not called since last update!");
    vassert(mUpdaters.size() == buffers.size(), "Invalid buffer count.");

    using namespace std::views;

    for (auto [idx, updater] : enumerate(mUpdaters))
    {
        updater.WriteUniformBuffer(binding, buffers[idx]);
    }
}

void DynamicDescriptorSet::WriteCombinedSampler(uint32_t binding, VkImageView imageView, VkSampler sampler)
{
    vassert(mInitialized, "Not initialized yet!");
    vassert(!mUpdaters.empty(), "BeginUpdate was not called since last update!");

    for (auto &updater : mUpdaters)
    {
        updater.WriteCombinedSampler(binding, imageView, sampler);
    }
}

void DynamicDescriptorSet::WriteCombinedSampler(
    uint32_t binding, std::span<VkImageView> imageViews, std::span<VkSampler> samplers
)
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
