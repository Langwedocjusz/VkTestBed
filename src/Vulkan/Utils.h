#pragma once

#include <vulkan/vulkan.h>

namespace utils
{
void BeginRecording(VkCommandBuffer buffer);
void EndRecording(VkCommandBuffer buffer);
} // namespace utils