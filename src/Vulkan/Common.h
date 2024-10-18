#pragma once

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
                                VkFence fence, VkSemaphore imageAcquiredSemaphore,
                                VkSemaphore renderCompleteSemaphore);

void AcquireNextImage(VulkanContext &ctx, VkSemaphore semaphore, uint32_t &imageIndex);

void PresentFrame(VulkanContext &ctx, VkQueue presentQueue,
                  VkSemaphore renderCompleteSemaphore, uint32_t &frameImageIndex);

void ImageBarrierColorToRender(VkCommandBuffer buffer, VkImage swapchainImage);
void ImageBarrierColorToPresent(VkCommandBuffer buffer, VkImage swapchainImage);

void ImageBarrierDepthToRender(VkCommandBuffer buffer, VkImage depthImage);
} // namespace common