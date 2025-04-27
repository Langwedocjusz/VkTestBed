#pragma once

#include "VulkanContext.h"

#include <vulkan/vulkan.h>

namespace vkutils
{
void BeginRecording(VkCommandBuffer buffer);
void EndRecording(VkCommandBuffer buffer);

/// Utility that creates a command buffer for single time
/// command exectuion and submits it at the end of scope.
class ScopedCommand {
  public:
    ScopedCommand(VulkanContext &ctx, VkQueue queue, VkCommandPool commandPool);
    ScopedCommand(const ScopedCommand &) = delete;

    ~ScopedCommand();

  public:
    VkCommandBuffer Buffer;

  private:
    VulkanContext &ctx;
    VkQueue mQueue;
    VkCommandPool mCommandPool;
};

template <typename HandleType>
inline void SetDebugName(VulkanContext &ctx, VkObjectType type, HandleType handle,
                         const std::string &name)
{
    VkDebugUtilsObjectNameInfoEXT debugLayoutInfo{};
    debugLayoutInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    debugLayoutInfo.objectType = type;
    // This is ugly, but the only way to use the extension (static cast is too
    // restrictive):
    debugLayoutInfo.objectHandle = (uint64_t)handle;
    debugLayoutInfo.pObjectName = name.c_str();

    ctx.SetDebugUtilsObjectName(ctx.Device.device, &debugLayoutInfo);
}

void BlitImage(VkCommandBuffer cmd, VkImage source, VkImage destination,
               VkExtent2D srcSize, VkExtent2D dstSize);
} // namespace vkutils