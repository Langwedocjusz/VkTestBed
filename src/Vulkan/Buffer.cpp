#include "Buffer.h"
#include "Pch.h"

#include "VkUtils.h"

#include "volk.h"

#include <cstring>

Buffer::Buffer(VulkanContext &ctx, const BufferInfo &info) : Size(info.Size), mCtx(ctx)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size  = info.Size;
    bufferInfo.usage = info.Usage;
    // Hardcoded for now:
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.usage                   = VMA_MEMORY_USAGE_AUTO;
    allocCreateInfo.flags                   = info.Flags;

    vmaCreateBuffer(ctx.Allocator, &bufferInfo, &allocCreateInfo, &Handle, &Allocation,
                    &AllocInfo);

    vkutils::SetDebugName(ctx, VK_OBJECT_TYPE_BUFFER, Handle, info.Name);
}

Buffer::~Buffer()
{
    vmaDestroyBuffer(mCtx.Allocator, Handle, Allocation);
}

Buffer::Buffer(Buffer &&other) noexcept
    : Handle(other.Handle), Size(other.Size), Allocation(other.Allocation),
      AllocInfo(other.AllocInfo), mCtx(other.mCtx)
{
    other.Handle     = {};
    other.Size       = {};
    other.Allocation = {};
    other.AllocInfo  = {};
}

void Buffer::Upload(const void *data, VkDeviceSize size)
{
    vmaCopyMemoryToAllocation(mCtx.Allocator, data, Allocation, 0, size);
}

void Buffer::UploadMapped(const void *data, VkDeviceSize size)
{
    std::memcpy(AllocInfo.pMappedData, data, size);
}

void Buffer::CopyContents(VkCommandBuffer cmd, const Buffer &src, Buffer &dst,
                          size_t size)
{
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size      = size;

    vkCmdCopyBuffer(cmd, src.Handle, dst.Handle, 1, &copyRegion);
}