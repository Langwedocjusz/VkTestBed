#pragma once

#include "VulkanContext.h"

#include "volk.h"

#include <deque>
#include <variant>

// clang-format off
using DeletionObject = std::variant<
    VkPipeline,
    VkPipelineLayout,
    VkCommandPool,
    VkFence,
    VkSemaphore,
    VkImageView,
    VkDescriptorPool,
    VkDescriptorSetLayout,
    VkSampler,
    VkQueryPool
>;
// clang-format on

class DeletionQueue {
  public:
    DeletionQueue(VulkanContext &ctx);
    ~DeletionQueue();

    void flush();

    template <typename T>
    void push_back(T &&obj)
    {
        mDeletionObjects.push_back(std::forward<T>(obj));
    }

  private:
    VulkanContext             &mCtx;
    std::deque<DeletionObject> mDeletionObjects;
};