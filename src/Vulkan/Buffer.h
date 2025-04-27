#pragma once

#include "VulkanContext.h"
#include <vulkan/vulkan_core.h>

struct CopyBufferInfo {
    VkBuffer Src;
    VkBuffer Dst;
    VkDeviceSize Size;
};

struct Buffer {
  public:
    static Buffer Create(VulkanContext &ctx, VkDeviceSize size, VkBufferUsageFlags usage,
                         VmaAllocationCreateFlags flags = 0);
    static void Destroy(VulkanContext &ctx, Buffer &buf);

    static void Upload(VulkanContext &ctx, Buffer buff, const void *data,
                       VkDeviceSize size);
    static void UploadToMapped(Buffer buff, const void *data, VkDeviceSize size);

    static void CopyBuffer(VkCommandBuffer cmd, CopyBufferInfo info);

  public:
    VkBuffer Handle;
    VmaAllocation Allocation;
    VmaAllocationInfo AllocInfo;
};