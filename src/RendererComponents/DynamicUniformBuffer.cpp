#include "DynamicUniformBuffer.h"
#include "Pch.h"

#include "Descriptor.h"

DynamicUniformBuffer::DynamicUniformBuffer(VulkanContext &ctx, FrameInfo &frame, VkDeviceSize bufferSize)
    : mCtx(ctx)
    , mFrame(frame)
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

DynamicDescriptorSet::DynamicDescriptorSet(
    VulkanContext &ctx, VkDescriptorPool staticPool, FrameInfo &frame, VkDescriptorSetLayout layout
)
    : mCtx(ctx)
    , mFrame(frame)
{
    mDescriptorSetLayout = layout;

    // Allocate descriptor sets:
    auto numDescriptors = static_cast<uint32_t>(mCtx.Swapchain->Images.size());

    std::vector<VkDescriptorSetLayout> layouts(numDescriptors, mDescriptorSetLayout);

    mDescriptorSets = Descriptor::Allocate(mCtx, staticPool, layouts);
}

VkDescriptorSetLayout DynamicDescriptorSet::DescriptorSetLayout() const
{
    return mDescriptorSetLayout;
}

VkDescriptorSet DynamicDescriptorSet::DescriptorSet() const
{
    return mDescriptorSets[mFrame.ImageIndex];
}

std::span<VkDescriptorSet> DynamicDescriptorSet::DescriptorSets()
{
    return {mDescriptorSets};
}