#pragma once

#include "RenderContext.h"
#include "VulkanContext.h"

#include <span>

namespace common
{
void ViewportScissorDefaultBehaviour(VulkanContext &ctx, VkCommandBuffer buffer);

void SubmitQueue(VkQueue queue, std::span<VkCommandBuffer> buffers, VkFence fence,
                 std::span<VkSemaphore> waitSemaphores,
                 std::span<VkPipelineStageFlags> waitStages,
                 std::span<VkSemaphore> signalSemaphores);

void SubmitGraphicsQueueDefault(VkQueue queue, std::span<VkCommandBuffer> buffers,
                                FrameData &frame);

void AcquireNextImage(VulkanContext &ctx, FrameInfo &frame);

void PresentFrame(VulkanContext &ctx, VkQueue presentQueue, FrameInfo& frame);
} // namespace common