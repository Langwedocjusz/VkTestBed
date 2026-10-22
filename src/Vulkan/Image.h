#pragma once

#include "DeletionQueue.h"
#include "ImageData.h"
#include "VulkanContext.h"

#include <optional>
#include <span>
#include <string>

struct Image;

struct Image2DInfo {
    std::string                  Name;
    VkExtent2D                   Extent;
    VkFormat                     Format;
    VkImageUsageFlags            Usage;
    uint32_t                     MipLevels     = 1;
    std::optional<VkImageLayout> Layout        = std::nullopt;
    VkSampleCountFlagBits        Multisampling = VK_SAMPLE_COUNT_1_BIT;
};

// Using a strict subrange of image that is not single
// layer/level is unusual so here the view will by default
// encompass the entire image, and single layer and/or
// level can be selected by explicitly overriding correspongind
// values. The aspect/format will by default reflect the format
// of the source Image. If you want for example to create
// depth-only view of a depth-stencil resource
// you then need to override it.
struct View2DInfo {
    Image                            &Img;
    std::optional<uint32_t>           SelectLevel    = std::nullopt;
    std::optional<uint32_t>           SelectLayer    = std::nullopt;
    std::optional<VkFormat>           FormatOverride = std::nullopt;
    std::optional<VkImageAspectFlags> AspectOverride = std::nullopt;
};

struct Image {
  public:
    VkImage           Handle;
    VkImageCreateInfo Info;
    VmaAllocation     Allocation;

  public:
    static Image Create(VulkanContext &ctx, const std::string &debugName,
                        VkImageCreateInfo &info);
    static void  Destroy(VulkanContext &ctx, Image &img);

    static Image Make2D(VulkanContext &ctx, const Image2DInfo &info);
    static Image Make2DArray(VulkanContext &ctx, const Image2DInfo &info,
                             uint32_t numLayers);
    static Image MakeCube(VulkanContext &ctx, const Image2DInfo &info);
    static Image FromData(VulkanContext &ctx, const std::string &debugName,
                          const ImageData &data);

    static VkImageView CreateView(VulkanContext &ctx, const std::string &debugName,
                                  VkImageViewCreateInfo &info);

    static VkImageView MakeView2D(VulkanContext &ctx, const std::string &debugName,
                                  View2DInfo info);
    static VkImageView MakeView2DArray(VulkanContext &ctx, const std::string &debugName,
                                       View2DInfo info);
    static VkImageView MakeViewCube(VulkanContext &ctx, const std::string &debugName,
                                    View2DInfo info);

    struct UploadInfo {
        const void             *Data;
        VkDeviceSize            Size;
        VkImageLayout           DstLayout;
        bool                    AllMips = false;
        std::span<const size_t> MipOffsets;
    };

    static void UploadToImage(VulkanContext &ctx, Image &img, UploadInfo info);

    static uint32_t MipCount(uint32_t size);
    static uint32_t MipCount(uint32_t width, uint32_t height);

    static void GenerateMips(VulkanContext &ctx, Image &img);

    static VkImageSubresourceRange GetDefaultRange(const Image &img);
};

struct Texture {
    Image       Img;
    VkImageView View;

    static Texture Make2D(VulkanContext &ctx, const Image2DInfo &info);
    static Texture Make2DArray(VulkanContext &ctx, const Image2DInfo &info,
                               uint32_t numLayers);
    static Texture MakeCube(VulkanContext &ctx, const Image2DInfo &info);
    static Texture FromData(VulkanContext &ctx, const std::string &debugName,
                            const ImageData &data);

    // Overloads that automatically push created resources to a given
    // deletion queue:

    static Texture Make2D(VulkanContext &ctx, const Image2DInfo &info,
                          DeletionQueue &queue);
    static Texture Make2DArray(VulkanContext &ctx, const Image2DInfo &info,
                               uint32_t numLayers, DeletionQueue &queue);
    static Texture MakeCube(VulkanContext &ctx, const Image2DInfo &info,
                            DeletionQueue &queue);
    static Texture FromData(VulkanContext &ctx, const std::string &debugName,
                            const ImageData &data, DeletionQueue &queue);
};