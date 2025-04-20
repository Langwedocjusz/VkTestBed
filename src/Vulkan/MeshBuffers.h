#pragma once

#include "Buffer.h"
#include "OpaqueBuffer.h"
#include "VulkanContext.h"

namespace VertexBuffer
{
inline Buffer Create(VulkanContext &ctx, VkQueue queue, VkCommandPool pool,
                     const OpaqueBuffer &buf)
{
    Buffer res;

    GPUBufferInfo vertInfo{
        .Ctx = ctx,
        .Queue = queue,
        .Pool = pool,
        .Usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .CreateFlags = 0,
        .Size = buf.Size,
        .Data = buf.Data.get(),
    };

    res = Buffer::CreateGPUBuffer(ctx, vertInfo);

    return res;
}
}; // namespace VertexBuffer

namespace IndexBuffer
{
inline Buffer Create(VulkanContext &ctx, VkQueue queue, VkCommandPool pool,
                     const OpaqueBuffer &buf)
{
    Buffer res;

    GPUBufferInfo idxInfo{
        .Ctx = ctx,
        .Queue = queue,
        .Pool = pool,
        .Usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        .CreateFlags = 0,
        .Size = buf.Size,
        .Data = buf.Data.get(),
    };

    res = Buffer::CreateGPUBuffer(ctx, idxInfo);

    return res;
}
} // namespace IndexBuffer