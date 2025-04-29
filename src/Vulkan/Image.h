#pragma once

#include "VulkanContext.h"

struct ImageUploadInfo {
    QueueType Queue;
    VkCommandPool Pool;
    const void *Data;
    VkDeviceSize Size;
    VkImageLayout DstLayout;
};

struct Image {
    static Image Create(VulkanContext &ctx, VkImageCreateInfo &info);
    static void Destroy(VulkanContext &ctx, Image &img);

    static VkImageView CreateView(VulkanContext &ctx, VkImageViewCreateInfo &info);

    static void UploadToImage(VulkanContext &ctx, Image &img, ImageUploadInfo info);

    static uint32_t CalcNumMips(uint32_t width, uint32_t height);
    static void GenerateMips(VulkanContext &ctx, QueueType queue, VkCommandPool pool,
                             Image &img);

    VkImage Handle;
    VkImageCreateInfo Info;
    VmaAllocation Allocation;
};
