#include "MakeBuffer.h"
#include "Pch.h"

Buffer MakeBuffer::Staging(VulkanContext &ctx, const std::string &debugName,
                           VkDeviceSize size)
{
    BufferInfo info{
        // NOTE: String copy
        .Name  = debugName,
        .Size  = size,
        .Usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .Flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
    };

    return Buffer{ctx, info};
}

Buffer MakeBuffer::MappedUniform(VulkanContext &ctx, const std::string &debugName,
                                 VkDeviceSize size)
{
    BufferInfo info{
        // NOTE: String copy
        .Name  = debugName,
        .Size  = size,
        .Usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        .Flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
    };

    return Buffer{ctx, info};
}

Buffer MakeBuffer::TransferDST(VulkanContext &ctx, const std::string &debugName,
                               TransferDSTInfo info)
{
    BufferInfo buffInfo{
        .Name  = debugName,
        .Size  = info.Size,
        .Usage = info.Usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .Flags = info.CreateFlags,
    };

    Buffer ret = Buffer{ctx, buffInfo};

    const auto stagingName   = debugName + "Staging";
    Buffer     stagingBuffer = MakeBuffer::Staging(ctx, stagingName, info.Size);

    stagingBuffer.Upload(info.Data, info.Size);

    // TODO: this should probably be done on transfer queue
    // if available:
    ctx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
        Buffer::CopyContents(cmd, stagingBuffer, ret, info.Size);
    });

    return ret;
}

Buffer MakeBuffer::Vertex(VulkanContext &ctx, const std::string &debugName,
                          const OpaqueBuffer &buf)
{
    TransferDSTInfo info{
        .Usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .CreateFlags = 0,
        .Size        = buf.Size,
        .Data        = buf.Data,
    };

    return MakeBuffer::TransferDST(ctx, debugName, info);
}

Buffer MakeBuffer::VertexStorage(VulkanContext &ctx, const std::string &debugName,
                                 const OpaqueBuffer &buf)
{
    TransferDSTInfo info{
        .Usage       = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                       VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .CreateFlags = 0,
        .Size        = buf.Size,
        .Data        = buf.Data,
    };

    return MakeBuffer::TransferDST(ctx, debugName, info);
}

Buffer MakeBuffer::Index(VulkanContext &ctx, const std::string &debugName,
                         const OpaqueBuffer &buf)
{
    TransferDSTInfo info{
        .Usage       = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        .CreateFlags = 0,
        .Size        = buf.Size,
        .Data        = buf.Data,
    };

    return MakeBuffer::TransferDST(ctx, debugName, info);
}