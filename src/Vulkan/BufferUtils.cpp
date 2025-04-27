#include "BufferUtils.h"

#include "VkUtils.h"

Buffer MakeBuffer::Staging(VulkanContext &ctx, VkDeviceSize size)
{
    auto usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    auto flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT;

    return Buffer::Create(ctx, size, usage, flags);
}

Buffer MakeBuffer::MappedUniform(VulkanContext &ctx, VkDeviceSize size)
{
    auto usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    auto flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT;

    return Buffer::Create(ctx, size, usage, flags);
}

Buffer MakeBuffer::TransferDST(VulkanContext &ctx, TransferDSTInfo info)
{
    const auto usage = info.Usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    Buffer buff = Buffer::Create(ctx, info.Size, usage, info.CreateFlags);

    Buffer stagingBuffer = MakeBuffer::Staging(ctx, info.Size);
    Buffer::Upload(ctx, stagingBuffer, info.Data, info.Size);

    {
        vkutils::ScopedCommand cmd(info.Ctx, info.Queue, info.Pool);

        CopyBufferInfo cp_info{
            .Src = stagingBuffer.Handle,
            .Dst = buff.Handle,
            .Size = info.Size,
        };

        Buffer::CopyBuffer(cmd.Buffer, cp_info);
    }

    Buffer::Destroy(ctx, stagingBuffer);

    return buff;
}

Buffer MakeBuffer::Vertex(VulkanContext &ctx, VkQueue queue, VkCommandPool pool,
                          const OpaqueBuffer &buf)
{
    Buffer res;

    TransferDSTInfo info{
        .Ctx = ctx,
        .Queue = queue,
        .Pool = pool,
        .Usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .CreateFlags = 0,
        .Size = buf.Size,
        .Data = buf.Data.get(),
    };

    res = MakeBuffer::TransferDST(ctx, info);

    return res;
}

Buffer MakeBuffer::Index(VulkanContext &ctx, VkQueue queue, VkCommandPool pool,
                         const OpaqueBuffer &buf)
{
    Buffer res;

    TransferDSTInfo info{
        .Ctx = ctx,
        .Queue = queue,
        .Pool = pool,
        .Usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        .CreateFlags = 0,
        .Size = buf.Size,
        .Data = buf.Data.get(),
    };

    res = MakeBuffer::TransferDST(ctx, info);

    return res;
}