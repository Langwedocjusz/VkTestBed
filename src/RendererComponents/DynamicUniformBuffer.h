#pragma once

#include "Buffer.h"
#include "DeletionQueue.h"
#include "Descriptor.h"
#include "Frame.h"
#include "VulkanContext.h"

// Class meant for uniform buffers with data updated each frame.
// Behind the scenes it juggles several (swapchain image count) buffers
// to avoid need for explicit synchronization.
// The buffers are exposed via descriptor sets created for them,
// which only reference the buffers.
class DynamicUniformBuffer {
  public:
    DynamicUniformBuffer(VulkanContext &ctx, FrameInfo &frame, VkDeviceSize bufSize);

    void UpdateData(void *data, VkDeviceSize size);

    std::span<Buffer> GetBuffers();

  private:
    std::vector<Buffer> mUniformBuffers;

    VulkanContext &mCtx;
    FrameInfo     &mFrame;
    DeletionQueue  mDeletionQueue;
};

class DynamicDescriptorSet {
  public:
    DynamicDescriptorSet(VulkanContext &ctx, FrameInfo &frame);

    // Pool counts corresponds to binding counts of
    // the given layout (as for one descriptor set).
    // Behind the scenes this will hold
    // (number of swapchain images) of the descriptors,
    // so it will actually allocate more.
    void Initialize(VkDescriptorSetLayout layout, Descriptor::PoolCounts counts);

    void BeginUpdate();
    void EndUpdate();

    void WriteUniformBuffer(uint32_t binding, Buffer &buffer);
    void WriteUniformBuffer(uint32_t binding, std::span<Buffer> buffers);

    void WriteCombinedSampler(uint32_t binding, VkImageView imageView, VkSampler sampler);
    void WriteCombinedSampler(uint32_t binding, std::span<VkImageView> imageViews, std::span<VkSampler> samplers);

    [[nodiscard]] VkDescriptorSetLayout DescriptorSetLayout() const;
    [[nodiscard]] VkDescriptorSet       DescriptorSet() const;

  private:
    bool mInitialized = false;

    VkDescriptorSetLayout        mDescriptorSetLayout;
    VkDescriptorPool             mDescriptorPool;
    std::vector<VkDescriptorSet> mDescriptorSets;

    std::vector<DescriptorUpdater> mUpdaters;

    VulkanContext &mCtx;
    FrameInfo     &mFrame;
    DeletionQueue  mDeletionQueue;
};