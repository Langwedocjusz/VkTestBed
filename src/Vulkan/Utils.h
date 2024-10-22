#pragma once

#include "VulkanContext.h"
#include <vulkan/vulkan.h>

namespace utils
{
void BeginRecording(VkCommandBuffer buffer);
void EndRecording(VkCommandBuffer buffer);

/// Utility that creates a command buffer for single time
/// command exectuion and submits it at the end of scope.
class ScopedCommand {
  public:
    ScopedCommand(VulkanContext &ctx, VkQueue queue, VkCommandPool commandPool);
    ~ScopedCommand();

  public:
    VkCommandBuffer Buffer;

  private:
    VulkanContext &ctx;
    VkQueue mQueue;
    VkCommandPool mCommandPool;
};

void BlitImage(VkCommandBuffer cmd, VkImage source, VkImage destination,
               VkExtent2D srcSize, VkExtent2D dstSize);
} // namespace utils