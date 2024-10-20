#pragma once

#include <vulkan/vulkan.h>

namespace utils
{
void BeginRecording(VkCommandBuffer buffer);
void EndRecording(VkCommandBuffer buffer);

void BlitImage(VkCommandBuffer cmd, VkImage source, VkImage destination,
               VkExtent2D srcSize, VkExtent2D dstSize);
} // namespace utils