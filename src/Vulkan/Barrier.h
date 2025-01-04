#pragma once

#include <vulkan/vulkan.h>

namespace barrier
{
void ImageBarrierColorToRender(VkCommandBuffer buffer, VkImage image);
void ImageBarrierColorToPresent(VkCommandBuffer buffer, VkImage image);

void ImageBarrierDepthToRender(VkCommandBuffer buffer, VkImage depthImage);

struct ImageLayoutBarrierInfo{
    VkImage Image;
    VkImageLayout OldLayout;
    VkImageLayout NewLayout;
    VkImageSubresourceRange SubresourceRange;
};

void ImageLayoutBarrierCoarse(VkCommandBuffer buffer, ImageLayoutBarrierInfo info);
} // namespace barrier