#include "BufferUtils.h"
#include "Pch.h"

Buffer MakeBuffer::Staging(VulkanContext &ctx, const std::string &debugName,
                           VkDeviceSize size)
{
    auto usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    auto flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT;

    return Buffer::Create(ctx, debugName, size, usage, flags);
}

Buffer MakeBuffer::MappedUniform(VulkanContext &ctx, const std::string &debugName,
                                 VkDeviceSize size)
{
    auto usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    auto flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT;

    return Buffer::Create(ctx, debugName, size, usage, flags);
}

Buffer MakeBuffer::TransferDST(VulkanContext &ctx, const std::string &debugName,
                               TransferDSTInfo info)
{
    const auto usage = info.Usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    Buffer buff = Buffer::Create(ctx, debugName, info.Size, usage, info.CreateFlags);

    const auto stagingName = debugName + "Staging";

    Buffer stagingBuffer = MakeBuffer::Staging(ctx, stagingName, info.Size);
    Buffer::Upload(ctx, stagingBuffer, info.Data, info.Size);

    // TODO: this should probably be done on transfer queue
    // if available:
    ctx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
        CopyBufferInfo cp_info{
            .Src = stagingBuffer.Handle,
            .Dst = buff.Handle,
            .Size = info.Size,
        };

        Buffer::CopyBuffer(cmd, cp_info);
    });

    Buffer::Destroy(ctx, stagingBuffer);

    return buff;
}

Buffer MakeBuffer::Vertex(VulkanContext &ctx, const std::string &debugName,
                          const OpaqueBuffer &buf)
{
    Buffer res;

    TransferDSTInfo info{
        .Usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .CreateFlags = 0,
        .Size = buf.Size,
        .Data = buf.Data,
    };

    res = MakeBuffer::TransferDST(ctx, debugName, info);

    return res;
}

Buffer MakeBuffer::Index(VulkanContext &ctx, const std::string &debugName,
                         const OpaqueBuffer &buf)
{
    Buffer res;

    TransferDSTInfo info{
        .Usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        .CreateFlags = 0,
        .Size = buf.Size,
        .Data = buf.Data,
    };

    res = MakeBuffer::TransferDST(ctx, debugName, info);

    return res;
}