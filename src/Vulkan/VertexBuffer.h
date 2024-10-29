#pragma once

#include "VulkanContext.h"
#include "Buffer.h"
#include "Vertex.h"
#include "Utils.h"

struct VertexBuffer{
public:
    template <Vertex V>
    static VertexBuffer Create(VulkanContext& ctx, VkQueue queue, VkCommandPool pool, const std::vector<V>& vertices)
    {
        Buffer buf;
        const auto vertexCount = vertices.size();

        {
            utils::ScopedCommand cmd(ctx, queue, pool);

            GPUBufferInfo vertInfo{
                .Usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                .Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                .Size = vertexCount * sizeof(V),
                .Data = vertices.data(),
            };

            buf = Buffer::CreateGPUBuffer(ctx, cmd.Buffer, vertInfo);
        }

        return VertexBuffer{
            .Handle = buf.Handle,
            .Allocation = buf.Allocation,
            .AllocInfo = buf.AllocInfo,
            .Count = vertexCount,
        };
    }

    static void Destroy(VulkanContext& ctx, VertexBuffer vert)
    {
        vmaDestroyBuffer(ctx.Allocator, vert.Handle, vert.Allocation);
    }
public:
    VkBuffer Handle;
    VmaAllocation Allocation;
    VmaAllocationInfo AllocInfo;
    size_t Count;
};