#pragma once

#include "VulkanContext.h"

struct ImageInfo {
    VkExtent3D Extent;
    VkFormat Format;
    VkImageTiling Tiling;
    VkImageUsageFlags Usage;
};

struct ImageUploadInfo {
    VkQueue Queue;
    VkCommandPool Pool;
    const void *Data;
    VkDeviceSize Size;
    VkImageLayout DstLayout;
};

struct Image {
    static Image CreateImage2D(VulkanContext &ctx, ImageInfo info);
    static Image CreateCubemap(VulkanContext &ctx, ImageInfo info);

    static void DestroyImage(VulkanContext &ctx, Image &img);

    static VkImageView CreateView2D(VulkanContext &ctx, Image &img, VkFormat format,
                                    VkImageAspectFlags aspectFlags);
    static VkImageView CreateViewCube(VulkanContext &ctx, Image &img, VkFormat format,
                                    VkImageAspectFlags aspectFlags);

    static void UploadToImage(VulkanContext &ctx, Image img, ImageUploadInfo info);

    VkImage Handle;
    VmaAllocation Allocation;
    ImageInfo Info;
};