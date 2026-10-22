#pragma once

#include "VulkanContext.h"

class Image;
struct Texture;

#include "volk.h"

#include <deque>
#include <variant>

struct VkAllocatedImage {
    VkImage       Handle;
    VmaAllocation Allocation;
};

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
    VkQueryPool,
    VkAllocatedImage
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

    void push_back(Image &img);
    void push_back(Texture &tex);

  private:
    VulkanContext             &mCtx;
    std::deque<DeletionObject> mDeletionObjects;
};