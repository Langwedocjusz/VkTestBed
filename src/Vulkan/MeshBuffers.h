#pragma once

#include "Buffer.h"
#include "GeometryProvider.h"
#include "Utils.h"
#include "VulkanContext.h"

namespace VertexBuffer
{
inline Buffer Create(VulkanContext &ctx, VkQueue queue, VkCommandPool pool,
                     OpaqueBuffer &buf)
{
    Buffer res;

    {
        utils::ScopedCommand cmd(ctx, queue, pool);

        GPUBufferInfo vertInfo{
            .Usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            .Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            .Size = buf.Size,
            .Data = buf.Data.get(),
        };

        res = Buffer::CreateGPUBuffer(ctx, cmd.Buffer, vertInfo);
    }

    return res;
}
}; // namespace VertexBuffer

namespace IndexBuffer
{
inline Buffer Create(VulkanContext &ctx, VkQueue queue, VkCommandPool pool,
                     OpaqueBuffer &buf)
{
    Buffer res;

    {
        utils::ScopedCommand cmd(ctx, queue, pool);

        GPUBufferInfo vertInfo{
            .Usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            .Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            .Size = buf.Size,
            .Data = buf.Data.get(),
        };

        res = Buffer::CreateGPUBuffer(ctx, cmd.Buffer, vertInfo);
    }

    return res;
}
} // namespace IndexBuffer