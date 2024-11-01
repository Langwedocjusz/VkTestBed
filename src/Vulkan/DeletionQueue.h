#pragma once

#include "Buffer.h"
#include "Image.h"
#include "VulkanContext.h"

#include <deque>
#include <variant>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

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
    //Custom aggregate types passed via pointer
    //to avoid excess padding in the variant:
    Image*,
    Buffer*
>;
// clang-format on

class DeletionQueue {
  public:
    DeletionQueue(VulkanContext &ctx) : mCtx(ctx) {};

    template <typename T>
    void push_back(T &&obj)
    {
        mDeletionObjects.push_back(std::forward<T>(obj));
    }

    void flush();

  private:
    VulkanContext &mCtx;
    std::deque<DeletionObject> mDeletionObjects;
};