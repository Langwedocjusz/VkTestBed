#pragma once

#include "Buffer.h"
#include "OpaqueBuffer.h"
#include "VulkanContext.h"

namespace MakeBuffer
{
struct TransferDSTInfo {
    QueueType Queue;
    VkCommandPool Pool;
    VkBufferUsageFlags Usage;
    VmaAllocationCreateFlags CreateFlags;
    VkDeviceSize Size;
    const void *Data;
};

Buffer Staging(VulkanContext &ctx, VkDeviceSize size);

Buffer MappedUniform(VulkanContext &ctx, VkDeviceSize size);

Buffer TransferDST(VulkanContext &ctx, TransferDSTInfo info);

Buffer Vertex(VulkanContext &ctx, QueueType queue, VkCommandPool pool,
              const OpaqueBuffer &buf);

Buffer Index(VulkanContext &ctx, QueueType queue, VkCommandPool pool,
             const OpaqueBuffer &buf);
} // namespace MakeBuffer
