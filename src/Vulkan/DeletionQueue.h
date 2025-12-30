#pragma once

#include "VulkanContext.h"

#include "Buffer.h"
#include "Image.h"
#include "Texture.h"

#include "volk.h"

#include <deque>
#include <variant>

struct VkAllocatedImage {
    VkImage Handle;
    VmaAllocation Allocation;
};

struct VkAllocatedBuffer {
    VkBuffer Handle;
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
    VkAllocatedImage,
    VkAllocatedBuffer
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

    void push_back(Buffer &buf)
    {
        mDeletionObjects.emplace_back(VkAllocatedBuffer{buf.Handle, buf.Allocation});
    }

    void push_back(Image &img)
    {
        mDeletionObjects.emplace_back(VkAllocatedImage{img.Handle, img.Allocation});
    }

    void push_back(Texture &tex)
    {
        mDeletionObjects.emplace_back(
            VkAllocatedImage{tex.Img.Handle, tex.Img.Allocation});
        mDeletionObjects.emplace_back(tex.View);
    }

    void flush();

  private:
    VulkanContext &mCtx;
    std::deque<DeletionObject> mDeletionObjects;
};