#include "Buffer.h"
#include "Pch.h"

#include "VkUtils.h"

#include "volk.h"

#include <cstring>

Buffer::Buffer(VulkanContext &ctx, const Buffer::Info &info) : Size(info.Size), mCtx(ctx)
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

Buffer Buffer::MakeStaging(VulkanContext &ctx, const Buffer::ThinInfo &info)
{
    Info fullInfo{
        // NOTE: String copy
        .Name  = info.Name,
        .Size  = info.Size,
        .Usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .Flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
    };

    return Buffer{ctx, fullInfo};
}

Buffer Buffer::MakeMappedUniform(VulkanContext &ctx, const Buffer::ThinInfo &info)
{
    Info fullInfo{
        // NOTE: String copy
        .Name  = info.Name,
        .Size  = info.Size,
        .Usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        .Flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
    };

    return Buffer{ctx, fullInfo};
}

Buffer Buffer::MakeTransferDST(VulkanContext &ctx, const Info &info, const void *data)
{
    // NOTE: Struct (including string) copy:
    Info localInfo = info;
    localInfo.Usage = localInfo.Usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    Buffer ret = Buffer{ctx, localInfo};

    ThinInfo stagingInfo{
        .Name = info.Name + "Staging",
        .Size = info.Size,
    };
    Buffer     stagingBuffer = Buffer::MakeStaging(ctx, stagingInfo);

    stagingBuffer.Upload(data, info.Size);

    // TODO: this should probably be done on transfer queue
    // if available:
    ctx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
        Buffer::CopyContents(cmd, stagingBuffer, ret, info.Size);
    });

    return ret;
}

Buffer Buffer::MakeVertex(VulkanContext &ctx, const std::string &name,
                          const OpaqueBuffer &buf)
{
    Info info{
        .Name = name,
        .Size = buf.Size,
        .Usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    };

    return MakeTransferDST(ctx, info, buf.Data);
}

Buffer Buffer::MakeVertexStorage(VulkanContext &ctx, const std::string &name,
                                 const OpaqueBuffer &buf)
{
    Info info{
        .Name = name,
        .Size = buf.Size,
        .Usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                       VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    };

    return MakeTransferDST(ctx, info, buf.Data);
}

Buffer Buffer::MakeIndex(VulkanContext &ctx, const std::string &name,
                         const OpaqueBuffer &buf)
{
    Info info{
        .Name = name,
        .Size = buf.Size,
        .Usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
    };

    return MakeTransferDST(ctx, info, buf.Data);
}