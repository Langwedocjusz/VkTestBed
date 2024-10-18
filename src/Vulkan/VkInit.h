#pragma once

#include "VulkanContext.h"

#include <span>

namespace vkinit
{
VkQueue CreateQueue(VulkanContext &ctx, vkb::QueueType type);

void CreateSignalledFence(VulkanContext &ctx, VkFence &fence);
void CreateSemaphore(VulkanContext &ctx, VkSemaphore &semaphore);

VkCommandPool CreateCommandPool(VulkanContext &ctx, vkb::QueueType qtype);
void AllocateCommandBuffers(VulkanContext &ctx, std::span<VkCommandBuffer> buffers,
                            VkCommandPool pool);
}; // namespace vkinit