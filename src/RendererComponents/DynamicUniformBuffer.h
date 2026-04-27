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

    using BufferHandles   = std::vector<VkBuffer>;
    using BufferSizes     = std::vector<VkDeviceSize>;
    using HandlesAndSizes = std::pair<BufferHandles, BufferSizes>;

    [[nodiscard]] HandlesAndSizes GetBufferHandlesAndSizes() const;

  private:
    VkDeviceSize        mBufferSize;
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
    void Initialize(VkDescriptorSetLayout layout, DescriptorBindingCounts poolCounts);

    void BeginUpdate();
    void EndUpdate();

    void WriteUniformBuffer(uint32_t binding, VkBuffer buffer, VkDeviceSize size);
    void WriteUniformBuffer(uint32_t binding, std::span<VkBuffer> buffers,
                            std::span<VkDeviceSize> sizes);

    void WriteCombinedSampler(uint32_t binding, VkImageView imageView, VkSampler sampler);
    void WriteCombinedSampler(uint32_t binding, std::span<VkImageView> imageViews,
                              std::span<VkSampler> samplers);

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