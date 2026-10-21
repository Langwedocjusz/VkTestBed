#pragma once

#include "VulkanContext.h"

#include "volk.h"

#include <string>

struct BufferInfo {
    std::string              Name;
    VkDeviceSize             Size  = 0;
    VkBufferUsageFlags       Usage = 0;
    VmaAllocationCreateFlags Flags = 0;
};

class Buffer {
  public:
    Buffer(VulkanContext &ctx, const BufferInfo &info);
    ~Buffer();
    Buffer(const Buffer &)            = delete;
    Buffer &operator=(const Buffer &) = delete;
    Buffer(Buffer &&) noexcept;
    Buffer &operator=(Buffer &&) = delete;

    void Upload(const void *data, VkDeviceSize size);
    // Will only worked on mapped buffers:
    void UploadMapped(const void *data, VkDeviceSize size);

    static void CopyContents(VkCommandBuffer cmd, const Buffer &src, Buffer &dst,
                             size_t size);

  public:
    VkBuffer          Handle;
    VkDeviceSize      Size;
    VmaAllocation     Allocation;
    VmaAllocationInfo AllocInfo;

  private:
    VulkanContext &mCtx;
};