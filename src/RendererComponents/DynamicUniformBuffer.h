#pragma once

#include "Buffer.h"
#include "DeletionQueue.h"
#include "Frame.h"
#include "VulkanContext.h"

// Class meant for uniform buffers with data updated each frame.
// Behind the scenes it juggles several (swapchain image count) buffers
// to avoid need for explicit synchronization.
class DynamicUniformBuffer {
  public:
    DynamicUniformBuffer(VulkanContext &ctx, FrameInfo &frame);
    ~DynamicUniformBuffer();

    void OnInit(std::string_view debugName, VkShaderStageFlags stageFlags,
                VkDeviceSize bufferSize);

    void UpdateData(void *data, VkDeviceSize size);

    [[nodiscard]] VkDescriptorSetLayout DescriptorSetLayout() const
    {
        return mDescriptorSetLayout;
    }

    [[nodiscard]] VkDescriptorSet *DescriptorSet()
    {
        return &mDescriptorSets[mFrame.ImageIndex];
    }

  private:
    bool mInitialized = false;

    VkDescriptorSetLayout mDescriptorSetLayout;
    VkDescriptorPool mDescriptorPool;
    std::vector<VkDescriptorSet> mDescriptorSets;

    std::vector<Buffer> mUniformBuffers;

    VulkanContext &mCtx;
    FrameInfo &mFrame;
    DeletionQueue mDeletionQueue;
};