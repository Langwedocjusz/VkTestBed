#pragma once

#include "VulkanContext.h"

#include <span>
#include <string>

struct Image {
  public:
    VkImage           Handle;
    VkImageCreateInfo Info;
    VmaAllocation     Allocation;

  public:
    static Image Create(VulkanContext &ctx, const std::string &debugName,
                        VkImageCreateInfo &info);
    static void  Destroy(VulkanContext &ctx, Image &img);

    static VkImageView CreateView(VulkanContext &ctx, const std::string &debugName,
                                  VkImageViewCreateInfo &info);

    struct UploadInfo {
        const void             *Data;
        VkDeviceSize            Size;
        VkImageLayout           DstLayout;
        bool                    AllMips = false;
        std::span<const size_t> MipOffsets;
    };

    static void UploadToImage(VulkanContext &ctx, Image &img, UploadInfo info);

    static uint32_t CalcNumMips(uint32_t size);
    static uint32_t CalcNumMips(uint32_t width, uint32_t height);

    static void GenerateMips(VulkanContext &ctx, Image &img);

    static VkImageSubresourceRange GetDefaultRange(const Image &img);
};
