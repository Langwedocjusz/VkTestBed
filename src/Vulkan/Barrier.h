#pragma once

#include <vulkan/vulkan.h>

namespace barrier
{
void ImageBarrierColorToRender(VkCommandBuffer buffer, VkImage image);
void ImageBarrierColorToPresent(VkCommandBuffer buffer, VkImage image);

void ImageBarrierDepthToRender(VkCommandBuffer buffer, VkImage depthImage);

void ImageLayoutBarrierCoarse(VkCommandBuffer buffer, VkImage image,
                              VkImageLayout oldLayout, VkImageLayout newLayout);
} // namespace barrier