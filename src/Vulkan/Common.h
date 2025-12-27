#pragma once

#include "Frame.h"
#include "VulkanContext.h"

#include <span>

namespace common
{
void ViewportScissor(VkCommandBuffer buffer, VkExtent2D extent);

void BeginRenderingColor(VkCommandBuffer cmd, VkExtent2D extent, VkImageView color,
                         bool clear);

void BeginRenderingColorDepth(VkCommandBuffer cmd, VkExtent2D extent, VkImageView color,
                              VkImageView depth, bool hasStencil, bool clear);

void BeginRenderingDepth(VkCommandBuffer cmd, VkExtent2D extent, VkImageView depth,
                         bool hasStencil, bool clear);

void SubmitQueue(VkQueue queue, VkCommandBuffer cmd, VkFence fence,
                 VkSemaphore waitSemaphore, VkPipelineStageFlags waitStage,
                 VkSemaphore signalSemaphore);

void SubmitQueue(VkQueue queue, std::span<VkCommandBuffer> buffers, VkFence fence,
                 std::span<VkSemaphore> waitSemaphores,
                 std::span<VkPipelineStageFlags> waitStages,
                 std::span<VkSemaphore> signalSemaphores);
} // namespace common