#pragma once

#include "VulkanContext.h"

struct ImageInfo {
    VkExtent3D Extent;
    VkFormat Format;
    VkImageTiling Tiling;
    VkImageUsageFlags Usage;
};

struct Image {
    static Image CreateImage2D(VulkanContext &ctx, ImageInfo info);
    static void DestroyImage(VulkanContext &ctx, Image &img);

    static VkImageView CreateView2D(VulkanContext &ctx, Image &img, VkFormat format,
                                    VkImageAspectFlags aspectFlags);

    VkImage Handle;
    VmaAllocation Allocation;
    ImageInfo Info;
};