#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace barrier
{
void ImageBarrier(VkCommandBuffer cmd, VkImageMemoryBarrier2 barrier);

// Assumes color target has layout VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
void ImageBarrierColorToRender(VkCommandBuffer cmd, VkImage image);
void ImageBarrierColorToTransfer(VkCommandBuffer cmd, VkImage image);

// Assumes swapchain image has layout: VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
void ImageBarrierSwapchainToTransfer(VkCommandBuffer cmd, VkImage image);
void ImageBarrierSwapchainToRender(VkCommandBuffer cmd, VkImage image);
void ImageBarrierSwapchainToPresent(VkCommandBuffer cmd, VkImage image);

// Assumes depth target has layout VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
void ImageBarrierDepthToRender(VkCommandBuffer cmd, VkImage depthImage);
void ImageBarrierDepthToSample(VkCommandBuffer cmd, VkImage depthImage);

struct ImageLayoutBarrierInfo {
    VkImage Image;
    VkImageLayout OldLayout;
    VkImageLayout NewLayout;
    VkImageSubresourceRange SubresourceRange;
};

void ImageLayoutBarrierCoarse(VkCommandBuffer cmd, ImageLayoutBarrierInfo info);
} // namespace barrier