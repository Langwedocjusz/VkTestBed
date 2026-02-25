#pragma once

#include "Buffer.h"
#include "OpaqueBuffer.h"
#include "VulkanContext.h"

#include <string>

namespace MakeBuffer
{
struct TransferDSTInfo {
    VkBufferUsageFlags       Usage;
    VmaAllocationCreateFlags CreateFlags;
    VkDeviceSize             Size;
    const void              *Data;
};

Buffer Staging(VulkanContext &ctx, const std::string &debugName, VkDeviceSize size);

Buffer MappedUniform(VulkanContext &ctx, const std::string &debugName, VkDeviceSize size);

Buffer TransferDST(VulkanContext &ctx, const std::string &debugName,
                   TransferDSTInfo info);

Buffer Vertex(VulkanContext &ctx, const std::string &debugName, const OpaqueBuffer &buf);
Buffer VertexStorage(VulkanContext &ctx, const std::string &debugName, const OpaqueBuffer &buf);
Buffer Index(VulkanContext &ctx, const std::string &debugName, const OpaqueBuffer &buf);
} // namespace MakeBuffer
