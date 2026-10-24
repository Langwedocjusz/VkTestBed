#pragma once

#include "ImageData.h"
#include "VulkanContext.h"

#include <optional>
#include <span>
#include <string>

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
    std::string                       Name;
    std::optional<uint32_t>           SelectLevel    = std::nullopt;
    std::optional<uint32_t>           SelectLayer    = std::nullopt;
    std::optional<VkFormat>           FormatOverride = std::nullopt;
    std::optional<VkImageAspectFlags> AspectOverride = std::nullopt;
};

struct ImageUploadInfo {
    const void             *Data;
    VkDeviceSize            Size;
    VkImageLayout           DstLayout;
    std::span<const size_t> MipOffsets;
};

class Image {
  public:
    ~Image();
    Image(const Image &)            = delete;
    Image &operator=(const Image &) = delete;
    Image(Image &&) noexcept;
    Image &operator=(Image &&) noexcept;

    // Factory functions:
    static Image Make2D(VulkanContext &ctx, const Image2DInfo &info);
    static Image Make2DArray(VulkanContext &ctx, uint32_t numLayers, const Image2DInfo &info);
    static Image MakeCube(VulkanContext &ctx, const Image2DInfo &info);
    static Image FromData(VulkanContext &ctx, const std::string &debugName, const ImageData &data);

    // Member functions:
    VkImageView              MakeView(const View2DInfo &info);
    std::vector<VkImageView> MakeViewsSingleMip();
    std::vector<VkImageView> MakeViewsSingleLayer();
    // Only works for Single2D Images atm:
    void                                  UploadData(const ImageUploadInfo &info);
    void                                  GenerateMips();
    [[nodiscard]] VkImageSubresourceRange GetDefaultRange() const;

    // Static helpers:
    static uint32_t MipCount(uint32_t size);
    static uint32_t MipCount(uint32_t width, uint32_t height);

  public:
    VkImage           Handle;
    VkImageCreateInfo Info;
    VmaAllocation     Allocation;
    std::string       DebugName;

  private:
    enum class InternalType : uint8_t
    {
        None,
        Single2D,
        Array2D,
        Cube
    };

    Image(VulkanContext &ctx, const Image2DInfo &info, uint32_t numLayers, VkImageCreateFlags flags, InternalType type);

    VulkanContext &mCtx;
    InternalType   mType;
};

class Texture {
  public:
    ~Texture();
    Texture(const Texture &)            = delete;
    Texture &operator=(const Texture &) = delete;
    Texture(Texture &&) noexcept;
    Texture &operator=(Texture &&) noexcept;

    // Factory functions:
    static Texture Make2D(VulkanContext &ctx, const Image2DInfo &info);
    static Texture Make2DArray(VulkanContext &ctx, uint32_t numLayers, const Image2DInfo &info);
    static Texture MakeCube(VulkanContext &ctx, const Image2DInfo &info);
    static Texture FromData(VulkanContext &ctx, const std::string &debugName, const ImageData &data);

    // Image& conversions:
    operator Image &() & noexcept
    {
        return Img;
    }
    operator const Image &() const & noexcept
    {
        return Img;
    }
    operator Image &() &&             = delete;
    operator const Image &() const && = delete;

  private:
    Texture(VulkanContext &ctx, Image &&img);

  public:
    Image       Img;
    VkImageView View;

  private:
    VulkanContext &mCtx;
};