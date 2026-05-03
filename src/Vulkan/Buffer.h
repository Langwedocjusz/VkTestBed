#pragma once

#include "VulkanContext.h"

#include "volk.h"

#include <string>

struct Buffer {
  public:
    VkBuffer          Handle;
    VmaAllocation     Allocation;
    VmaAllocationInfo AllocInfo;

  public:
    static Buffer Create(VulkanContext &ctx, const std::string &debugName,
                         VkDeviceSize size, VkBufferUsageFlags usage,
                         VmaAllocationCreateFlags flags = 0);
    static void   Destroy(VulkanContext &ctx, Buffer &buf);

    static void Upload(VulkanContext &ctx, Buffer buff, const void *data,
                       VkDeviceSize size);
    static void UploadToMapped(Buffer buff, const void *data, VkDeviceSize size);

    struct CopyInfo {
        VkBuffer     Src;
        VkBuffer     Dst;
        VkDeviceSize Size;
    };

    static void CopyBuffer(VkCommandBuffer cmd, CopyInfo info);
};