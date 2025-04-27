#pragma once

#include "DeletionQueue.h"
#include "Image.h"
#include "Texture.h"

struct Image2DInfo {
    VkExtent2D Extent;
    VkFormat Format;
    VkImageTiling Tiling;
    VkImageUsageFlags Usage;
    uint32_t MipLevels;
};

namespace MakeImage
{
Image Image2D(VulkanContext &ctx, Image2DInfo info);
Image Cube(VulkanContext &ctx, Image2DInfo info);
} // namespace MakeImage

namespace MakeView
{
VkImageView View2D(VulkanContext &ctx, Image &img, VkFormat format,
                   VkImageAspectFlags aspectFlags);
VkImageView ViewCube(VulkanContext &ctx, Image &img, VkFormat format,
                     VkImageAspectFlags aspectFlags);

VkImageView ViewCubeSingleMip(VulkanContext &ctx, Image &img, VkFormat format,
                              VkImageAspectFlags aspectFlags, uint32_t mip);
} // namespace MakeView

namespace MakeTexture
{
Texture Texture2D(VulkanContext &ctx, Image2DInfo info);
Texture Texture2D(VulkanContext &ctx, Image2DInfo info, DeletionQueue &queue);

Texture TextureCube(VulkanContext &ctx, Image2DInfo info);
Texture TextureCube(VulkanContext &ctx, Image2DInfo info, DeletionQueue &queue);
} // namespace MakeTexture