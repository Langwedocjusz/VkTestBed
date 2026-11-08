#pragma once

#include "Image.h"
#include "VulkanContext.h"

#include "volk.h"

#include <span>

namespace vkutils
{
template <typename HandleType>
inline void SetDebugName(VulkanContext &ctx, VkObjectType type, HandleType handle, const std::string &name)
{
    VkDebugUtilsObjectNameInfoEXT debugLayoutInfo{};
    debugLayoutInfo.sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    debugLayoutInfo.objectType = type;
    // Using ugly C-style cast here, since to use extension,
    // multiple handle types (some pointers, some not)
    // need to be cast to uint64. So at the minimum I would
    // need to do constexpr branch and combine
    // reinterpret and static casts myself:
    debugLayoutInfo.objectHandle = (uint64_t)handle;
    debugLayoutInfo.pObjectName  = name.c_str();

    vkSetDebugUtilsObjectNameEXT(ctx.Device, &debugLayoutInfo);
}

// Sync object creation:
void CreateSignalledFence(VulkanContext &ctx, VkFence &fence);
void CreateSemaphore(VulkanContext &ctx, VkSemaphore &semaphore);

// Command Buffer management:
VkCommandPool CreateCommandPool(VulkanContext &ctx, QueueType qtype);

VkCommandBuffer AllocateCommandBuffer(VulkanContext &ctx, VkCommandPool pool);

void AllocateCommandBuffers(VulkanContext &ctx, std::span<VkCommandBuffer> buffers, VkCommandPool pool);

void BeginRecording(VkCommandBuffer buffer, VkCommandBufferUsageFlags flags = 0);
void EndRecording(VkCommandBuffer buffer);

// Image/Format utils:
bool IsFormatSRGB(VkFormat format);

VkImageAspectFlags GetDefaultAspect(VkFormat format);

struct BlitImageInfo {
    VkImage    ImgHandle;
    VkExtent3D Extent;
};

void BlitImageZeroMip(VkCommandBuffer cmd, const Image &src, const Image &dst);
void BlitImageZeroMip(VkCommandBuffer cmd, const Image &src, BlitImageInfo dst);
} // namespace vkutils