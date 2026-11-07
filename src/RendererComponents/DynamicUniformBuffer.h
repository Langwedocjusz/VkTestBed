#pragma once

#include "Buffer.h"
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
};

class DynamicDescriptorSet {
  public:
    DynamicDescriptorSet(
        VulkanContext &ctx, VkDescriptorPool staticPool, FrameInfo &frame, VkDescriptorSetLayout layout
    );

    [[nodiscard]] VkDescriptorSetLayout DescriptorSetLayout() const;
    // Returns the *current* descriptor set:
    [[nodiscard]] VkDescriptorSet DescriptorSet() const;
    // Returns view of *all* descriptor sets:
    [[nodiscard]] std::span<VkDescriptorSet> DescriptorSets();

  private:
    VkDescriptorSetLayout        mDescriptorSetLayout;
    std::vector<VkDescriptorSet> mDescriptorSets;

    VulkanContext &mCtx;
    FrameInfo     &mFrame;
};