#pragma once

#include "VulkanContext.h"

#include <optional>
#include <span>
#include <vulkan/vulkan_core.h>

namespace vkinit
{
VkQueue CreateQueue(VulkanContext &ctx, vkb::QueueType type);

void CreateSignalledFence(VulkanContext &ctx, VkFence &fence);
void CreateSemaphore(VulkanContext &ctx, VkSemaphore &semaphore);

VkCommandPool CreateCommandPool(VulkanContext &ctx, vkb::QueueType qtype);
VkCommandBuffer CreateCommandBuffer(VulkanContext &ctx, VkCommandPool pool);

void AllocateCommandBuffers(VulkanContext &ctx, std::span<VkCommandBuffer> buffers,
                            VkCommandPool pool);

VkRenderingAttachmentInfoKHR CreateAttachmentInfo(VkImageView view, VkImageLayout layout,
                                                  std::optional<VkClearValue> clear);

VkRenderingInfoKHR CreateRenderingInfo(VkExtent2D extent,
                                       VkRenderingAttachmentInfo &colorAttachment);

VkRenderingInfoKHR CreateRenderingInfo(VkExtent2D extent,
                                       VkRenderingAttachmentInfo &colorAttachment,
                                       VkRenderingAttachmentInfo &depthAttachment);
}; // namespace vkinit