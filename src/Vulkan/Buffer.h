#pragma once

#include "VulkanContext.h"
#include <vulkan/vulkan_core.h>

struct CopyBufferInfo {
    VkBuffer Src;
    VkBuffer Dst;
    VkDeviceSize Size;
};

struct GPUBufferInfo {
    VulkanContext &Ctx;
    VkQueue Queue;
    VkCommandPool Pool;
    VkBufferUsageFlags Usage;
    VmaAllocationCreateFlags CreateFlags;
    VkDeviceSize Size;
    const void *Data;
};

struct Buffer {
  public:
    static Buffer CreateBuffer(VulkanContext &ctx, VkDeviceSize size,
                               VkBufferUsageFlags usage,
                               VmaAllocationCreateFlags flags = 0);
    static void DestroyBuffer(VulkanContext &ctx, Buffer &buf);

    static void UploadToBuffer(VulkanContext &ctx, Buffer buff, const void *data,
                               VkDeviceSize size);
    static void UploadToMappedBuffer(Buffer buff, const void *data, VkDeviceSize size);

    static Buffer CreateStagingBuffer(VulkanContext &ctx, VkDeviceSize size);
    static Buffer CreateMappedUniformBuffer(VulkanContext &ctx, VkDeviceSize size);
    static Buffer CreateGPUBuffer(VulkanContext &ctx, GPUBufferInfo info);

    static void CopyBuffer(VkCommandBuffer cmd, CopyBufferInfo info);

  public:
    VkBuffer Handle;
    VmaAllocation Allocation;
    VmaAllocationInfo AllocInfo;
};