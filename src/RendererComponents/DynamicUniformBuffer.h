#pragma once

#include "Buffer.h"
#include "DeletionQueue.h"
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

    void Initialize(std::string_view debugName, VkShaderStageFlags stageFlags);

    void UpdateData(void *data, VkDeviceSize size);

    [[nodiscard]] VkDescriptorSetLayout DescriptorSetLayout() const;
    [[nodiscard]] VkDescriptorSet       DescriptorSet() const;

  private:
    bool mInitialized = false;

    VkDeviceSize        mBufferSize;
    std::vector<Buffer> mUniformBuffers;

    VkDescriptorSetLayout        mDescriptorSetLayout;
    VkDescriptorPool             mDescriptorPool;
    std::vector<VkDescriptorSet> mDescriptorSets;

    VulkanContext &mCtx;
    FrameInfo     &mFrame;
    DeletionQueue  mDeletionQueue;
};