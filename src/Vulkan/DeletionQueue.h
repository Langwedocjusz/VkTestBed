#pragma once

#include "Image.h"
#include "VulkanContext.h"

#include <deque>
#include <variant>

#include <vulkan/vulkan.h>

// clang-format off
using DeletionObject = std::variant<
    VkPipeline,
    VkPipelineLayout,
    VkCommandPool,
    VkFence,
    VkSemaphore,
    Image*,
    VkImageView
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