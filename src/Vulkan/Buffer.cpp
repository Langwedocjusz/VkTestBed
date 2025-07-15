#include "Buffer.h"
#include "Pch.h"

#include "VkUtils.h"

#include <vulkan/vulkan.h>

Buffer Buffer::Create(VulkanContext &ctx, const std::string &debugName, VkDeviceSize size,
                      VkBufferUsageFlags usage, VmaAllocationCreateFlags flags)
{
    Buffer buf;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    // Hardcoded for now:
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocCreateInfo.flags = flags;

    vmaCreateBuffer(ctx.Allocator, &bufferInfo, &allocCreateInfo, &buf.Handle,
                    &buf.Allocation, &buf.AllocInfo);

    vkutils::SetDebugName(ctx, VK_OBJECT_TYPE_BUFFER, buf.Handle, debugName);

    return buf;
}

void Buffer::Destroy(VulkanContext &ctx, Buffer &buf)
{
    vmaDestroyBuffer(ctx.Allocator, buf.Handle, buf.Allocation);
}

void Buffer::Upload(VulkanContext &ctx, Buffer buff, const void *data, VkDeviceSize size)
{
    vmaCopyMemoryToAllocation(ctx.Allocator, data, buff.Allocation, 0, size);
}

void Buffer::UploadToMapped(Buffer buff, const void *data, VkDeviceSize size)
{
    std::memcpy(buff.AllocInfo.pMappedData, data, size);
}

void Buffer::CopyBuffer(VkCommandBuffer cmd, CopyBufferInfo info)
{
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = info.Size;

    vkCmdCopyBuffer(cmd, info.Src, info.Dst, 1, &copyRegion);
}