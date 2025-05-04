#pragma once

#include "VulkanContext.h"

#include <string>

struct ImageUploadInfo {
    const void *Data;
    VkDeviceSize Size;
    VkImageLayout DstLayout;
};

struct Image {
    static Image Create(VulkanContext &ctx, const std::string &debugName,
                        VkImageCreateInfo &info);
    static void Destroy(VulkanContext &ctx, Image &img);

    static VkImageView CreateView(VulkanContext &ctx, const std::string &debugName,
                                  VkImageViewCreateInfo &info);

    static void UploadToImage(VulkanContext &ctx, Image &img, ImageUploadInfo info);

    static uint32_t CalcNumMips(uint32_t width, uint32_t height);
    static void GenerateMips(VulkanContext &ctx, Image &img);

    VkImage Handle;
    VkImageCreateInfo Info;
    VmaAllocation Allocation;
};
