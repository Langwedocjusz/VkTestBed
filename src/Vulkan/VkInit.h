#pragma once

#include "VulkanContext.h"

#include "volk.h"

#include <span>

namespace vkinit
{
void CreateSignalledFence(VulkanContext &ctx, VkFence &fence);
void CreateSemaphore(VulkanContext &ctx, VkSemaphore &semaphore);

VkCommandPool   CreateCommandPool(VulkanContext &ctx, vkb::QueueType qtype);
VkCommandBuffer CreateCommandBuffer(VulkanContext &ctx, VkCommandPool pool);

void AllocateCommandBuffers(VulkanContext &ctx, std::span<VkCommandBuffer> buffers,
                            VkCommandPool pool);
}; // namespace vkinit