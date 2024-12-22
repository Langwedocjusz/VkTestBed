#include "Buffer.h"
#include "DeletionQueue.h"

#include <vulkan/vulkan.h>

Buffer Buffer::CreateBuffer(VulkanContext &ctx, VkDeviceSize size,
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

    return buf;
}

void Buffer::DestroyBuffer(VulkanContext &ctx, Buffer &buf)
{
    vmaDestroyBuffer(ctx.Allocator, buf.Handle, buf.Allocation);
}

void Buffer::UploadToBuffer(VulkanContext &ctx, Buffer buff, const void *data,
                            VkDeviceSize size)
{
    vmaCopyMemoryToAllocation(ctx.Allocator, data, buff.Allocation, 0, size);
}

void Buffer::UploadToMappedBuffer(Buffer buff, const void *data, VkDeviceSize size)
{
    std::memcpy(buff.AllocInfo.pMappedData, data, size);
}

Buffer Buffer::CreateStagingBuffer(VulkanContext &ctx, VkDeviceSize size)
{
    auto usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    auto flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT;

    return CreateBuffer(ctx, size, usage, flags);
}

Buffer Buffer::CreateMappedUniformBuffer(VulkanContext &ctx, VkDeviceSize size)
{
    auto usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    auto flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT;

    return CreateBuffer(ctx, size, usage, flags);
}

Buffer Buffer::CreateGPUBuffer(VulkanContext &ctx, VkCommandBuffer cmd,
                               GPUBufferInfo info)
{
    const auto usage = info.Usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    Buffer buff;
    buff = CreateBuffer(ctx, info.Size, usage, info.Properties);

    Buffer stagingBuffer = Buffer::CreateStagingBuffer(ctx, info.Size);
    UploadToBuffer(ctx, stagingBuffer, info.Data, info.Size);

    CopyBufferInfo cp_info{
        .Src = stagingBuffer.Handle,
        .Dst = buff.Handle,
        .Size = info.Size,
    };

    CopyBuffer(cmd, cp_info);

    DestroyBuffer(ctx, stagingBuffer);

    return buff;
}

void Buffer::CopyBuffer(VkCommandBuffer cmd, CopyBufferInfo info)
{
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = info.Size;
    vkCmdCopyBuffer(cmd, info.Src, info.Dst, 1, &copyRegion);
}