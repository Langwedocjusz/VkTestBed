#pragma once

#include "Frame.h"
#include "VulkanContext.h"

#include <span>

namespace common
{
void ViewportScissor(VkCommandBuffer buffer, VkExtent2D extent);

void SubmitQueue(VkQueue queue, std::span<VkCommandBuffer> buffers, VkFence fence,
                 std::span<VkSemaphore> waitSemaphores,
                 std::span<VkPipelineStageFlags> waitStages,
                 std::span<VkSemaphore> signalSemaphores);

void SubmitQueue(VkQueue queue, VkCommandBuffer *cmdBuffer, VkFence fence,
                 VkSemaphore *waitSemaphore, VkPipelineStageFlags *waitStage,
                 VkSemaphore *signalSemaphore);
} // namespace common