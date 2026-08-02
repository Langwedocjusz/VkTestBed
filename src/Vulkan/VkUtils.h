#pragma once

#include "Image.h"
#include "VulkanContext.h"

#include "volk.h"

namespace vkutils
{
template <typename HandleType>
inline void SetDebugName(VulkanContext &ctx, VkObjectType type, HandleType handle,
                         const std::string &name)
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

void BeginRecording(VkCommandBuffer buffer, VkCommandBufferUsageFlags flags = 0);
void EndRecording(VkCommandBuffer buffer);

VkImageAspectFlags GetDefaultAspect(VkFormat format);

struct BlitImageInfo {
    VkImage    ImgHandle;
    VkExtent3D Extent;
    uint32_t   NumLayers;
};

void BlitImageZeroMip(VkCommandBuffer cmd, const Image &src, const Image &dst);
void BlitImageZeroMip(VkCommandBuffer cmd, const Image &src, BlitImageInfo dst);
} // namespace vkutils