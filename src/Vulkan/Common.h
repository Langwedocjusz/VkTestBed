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

void SubmitGraphicsQueue(VulkanContext &ctx, std::span<VkCommandBuffer> buffers, FrameData &frame);

void SubmitGraphicsQueue(VulkanContext &ctx, VkCommandBuffer buffer,
                         FrameData &frame);

void AcquireNextImage(VulkanContext &ctx, FrameInfo &frame);

void PresentFrame(VulkanContext &ctx, FrameInfo &frame);
} // namespace common