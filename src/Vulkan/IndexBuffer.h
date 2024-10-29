#pragma once

#include "VulkanContext.h"
#include "Buffer.h"
#include "GeometryProvider.h"
#include "Utils.h"
#include <vulkan/vulkan_core.h>

struct IndexBuffer{
public:
    template <ValidIndexType I>
    static IndexBuffer Create(VulkanContext& ctx, VkQueue queue, VkCommandPool pool, const std::vector<I>& indices)
    {
        Buffer buf;
        const auto indexCount = indices.size();

        {
            utils::ScopedCommand cmd(ctx, queue, pool);

            GPUBufferInfo vertInfo{
                .Usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                .Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                .Size = indexCount * sizeof(I),
                .Data = indices.data(),
            };

            buf = Buffer::CreateGPUBuffer(ctx, cmd.Buffer, vertInfo);
        }

        VkIndexType idxType;

        if constexpr (std::same_as<I, uint16_t>)
            idxType = VK_INDEX_TYPE_UINT16;
        else if constexpr(std::same_as<I, uint32_t>)
            idxType = VK_INDEX_TYPE_UINT32;
        else
            static_assert(false, "Unsupported index type!");

        return IndexBuffer{
            .Handle = buf.Handle,
            .Allocation = buf.Allocation,
            .AllocInfo = buf.AllocInfo,
            .IndexType = idxType,
            .Count = indexCount,
        };
    }

    static void Destroy(VulkanContext& ctx, IndexBuffer vert)
    {
        vmaDestroyBuffer(ctx.Allocator, vert.Handle, vert.Allocation);
    }

    void Bind(VkCommandBuffer cmd, VkDeviceSize offset)
    {
        vkCmdBindIndexBuffer(cmd, Handle, offset, IndexType);
    }
public:
    VkBuffer Handle;
    VmaAllocation Allocation;
    VmaAllocationInfo AllocInfo;
    VkIndexType IndexType;
    size_t Count;
};