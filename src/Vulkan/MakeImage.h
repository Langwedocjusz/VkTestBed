#pragma once

#include "DeletionQueue.h"
#include "Image.h"
#include "ImageData.h"
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

Image FromData(VulkanContext &ctx, const std::string &debugName, const ImageData &data);
} // namespace MakeImage

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

namespace MakeView
{
VkImageView View2D(VulkanContext &ctx, const std::string &debugName, View2DInfo info);
VkImageView View2DArray(VulkanContext &ctx, const std::string &debugName,
                        View2DInfo info);
VkImageView ViewCube(VulkanContext &ctx, const std::string &debugName, View2DInfo info);
} // namespace MakeView

namespace MakeTexture
{
Texture Texture2D(VulkanContext &ctx, const std::string &debugName, Image2DInfo info);
Texture Texture2DArray(VulkanContext &ctx, const std::string &debugName, Image2DInfo info,
                       uint32_t numLayers);
Texture TextureCube(VulkanContext &ctx, const std::string &debugName, Image2DInfo info);
Texture FromData(VulkanContext &ctx, const std::string &debugName, const ImageData &data);

// Overloads that automatically push created resources to a given
// deletion queue:

Texture Texture2D(VulkanContext &ctx, const std::string &debugName, Image2DInfo info,
                  DeletionQueue &queue);
Texture Texture2DArray(VulkanContext &ctx, const std::string &debugName, Image2DInfo info,
                       uint32_t numLayers, DeletionQueue &queue);
Texture TextureCube(VulkanContext &ctx, const std::string &debugName, Image2DInfo info,
                    DeletionQueue &queue);
Texture FromData(VulkanContext &ctx, const std::string &debugName, const ImageData &data,
                 DeletionQueue &queue);
} // namespace MakeTexture