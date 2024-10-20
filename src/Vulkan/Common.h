#pragma once

#include "RenderContext.h"
#include "VulkanContext.h"

#include <span>

namespace common
{
void ViewportScissor(VkCommandBuffer buffer, VkExtent2D extent);

void SubmitQueue(VkQueue queue, std::span<VkCommandBuffer> buffers, VkFence fence,
                 std::span<VkSemaphore> waitSemaphores,
                 std::span<VkPipelineStageFlags> waitStages,
                 std::span<VkSemaphore> signalSemaphores);

void SubmitGraphicsQueue(RenderContext::Queues &queues,
                         std::span<VkCommandBuffer> buffers, FrameData &frame);

void SubmitGraphicsQueue(RenderContext::Queues &queues, VkCommandBuffer buffer,
                         FrameData &frame);

void AcquireNextImage(VulkanContext &ctx, FrameInfo &frame);

void PresentFrame(VulkanContext &ctx, RenderContext::Queues &queues, FrameInfo &frame);
} // namespace common