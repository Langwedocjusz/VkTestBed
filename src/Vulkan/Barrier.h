#pragma once

#include <vulkan/vulkan.h>

namespace barrier
{
void ImageBarrierColorToRender(VkCommandBuffer buffer, VkImage image);
void ImageBarrierColorToPresent(VkCommandBuffer buffer, VkImage image);
} // namespace barrier