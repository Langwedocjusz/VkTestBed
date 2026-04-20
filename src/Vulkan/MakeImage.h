#pragma once

#include "DeletionQueue.h"
#include "Image.h"
#include "Texture.h"

#include "volk.h"

#include <optional>
#include <string>

struct Image2DInfo {
    VkExtent2D                   Extent;
    VkFormat                     Format;
    VkImageUsageFlags            Usage;
    uint32_t                     MipLevels     = 1;
    std::optional<VkImageLayout> Layout        = std::nullopt;
    VkSampleCountFlagBits        Multisampling = VK_SAMPLE_COUNT_1_BIT;
};

namespace MakeImage
{
Image Image2D(VulkanContext &ctx, const std::string &debugName, Image2DInfo info);

Image Image2DArray(VulkanContext &ctx, const std::string &debugName, Image2DInfo info,
                   uint32_t numLayers);

Image Cube(VulkanContext &ctx, const std::string &debugName, Image2DInfo info);
} // namespace MakeImage

namespace MakeView
{
VkImageView View2D(VulkanContext &ctx, const std::string &debugName, Image &img,
                   VkFormat format, VkImageAspectFlags aspectFlags);

VkImageView View2DArray(VulkanContext &ctx, const std::string &debugName, Image &img,
                        VkFormat format, VkImageAspectFlags aspectFlags);

VkImageView ViewCube(VulkanContext &ctx, const std::string &debugName, Image &img,
                     VkFormat format, VkImageAspectFlags aspectFlags);

VkImageView ViewArraySingleLayer(VulkanContext &ctx, const std::string &debugName,
                                 Image &img, VkFormat format,
                                 VkImageAspectFlags aspectFlags, uint32_t layer);

VkImageView ViewCubeSingleMip(VulkanContext &ctx, const std::string &debugName,
                              Image &img, VkFormat format, VkImageAspectFlags aspectFlags,
                              uint32_t mip);
} // namespace MakeView

namespace MakeTexture
{
Texture Texture2D(VulkanContext &ctx, const std::string &debugName, Image2DInfo info);

Texture Texture2D(VulkanContext &ctx, const std::string &debugName, Image2DInfo info,
                  DeletionQueue &queue);

Texture Texture2DArray(VulkanContext &ctx, const std::string &debugName, Image2DInfo info,
                       uint32_t numLayers);

Texture Texture2DArray(VulkanContext &ctx, const std::string &debugName, Image2DInfo info,
                       uint32_t numLayers, DeletionQueue &queue);

Texture TextureCube(VulkanContext &ctx, const std::string &debugName, Image2DInfo info);

Texture TextureCube(VulkanContext &ctx, const std::string &debugName, Image2DInfo info,
                    DeletionQueue &queue);
} // namespace MakeTexture