#include "ImageUtils.h"
#include "Pch.h"

#include "Barrier.h"
#include "VkUtils.h"

#include "volk.h"

static VkExtent3D Extent2DTo3D(VkExtent2D extent)
{
    return VkExtent3D{
        .width  = extent.width,
        .height = extent.height,
        .depth  = 1,
    };
}

Image MakeImage::Image2D(VulkanContext &ctx, const std::string &debugName,
                         Image2DInfo info)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType     = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent    = Extent2DTo3D(info.Extent);
    imageInfo.format    = info.Format;
    imageInfo.usage     = info.Usage;
    imageInfo.mipLevels = info.MipLevels;
    // Multisampling, only relevant for attachments:
    imageInfo.samples = info.Multisampling;

    // Hardcoded part:
    imageInfo.flags       = 0;
    imageInfo.arrayLayers = 1;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    // This is actual order of pixels in memory, not sampler tiling:
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    // Only other option is PREINITIALIZED:
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    auto res = Image::Create(ctx, debugName, imageInfo);

    // If specific layout was provided - immediately transition the image:
    if (info.Layout.has_value())
    {
        ctx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
            auto barrierInfo = barrier::LayoutTransitionInfo{
                .Image            = res,
                .OldLayout        = VK_IMAGE_LAYOUT_UNDEFINED,
                .NewLayout        = *info.Layout,
            };

            barrier::ImageLayoutCoarse(cmd, barrierInfo);
        });
    }

    return res;
}

Image MakeImage::Image2DArray(VulkanContext &ctx, const std::string &debugName,
                              Image2DInfo info, uint32_t numLayers)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType   = VK_IMAGE_TYPE_2D;
    imageInfo.extent      = Extent2DTo3D(info.Extent);
    imageInfo.format      = info.Format;
    imageInfo.usage       = info.Usage;
    imageInfo.mipLevels   = info.MipLevels;
    imageInfo.arrayLayers = numLayers;
    imageInfo.samples     = info.Multisampling;

    // Hardcoded part:
    imageInfo.flags         = 0;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    auto res = Image::Create(ctx, debugName, imageInfo);

    // If specific layout was provided - immediately transition the image:
    if (info.Layout.has_value())
    {
        ctx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
            auto barrierInfo = barrier::LayoutTransitionInfo{
                .Image            = res,
                .OldLayout        = VK_IMAGE_LAYOUT_UNDEFINED,
                .NewLayout        = *info.Layout,
            };

            barrier::ImageLayoutCoarse(cmd, barrierInfo);
        });
    }

    return res;
}

Image MakeImage::Cube(VulkanContext &ctx, const std::string &debugName, Image2DInfo info)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType     = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent    = Extent2DTo3D(info.Extent);
    imageInfo.format    = info.Format;
    imageInfo.usage     = info.Usage;
    imageInfo.mipLevels = info.MipLevels;
    imageInfo.samples   = info.Multisampling;

    // Hardcoded part:
    imageInfo.arrayLayers   = 6;
    imageInfo.flags         = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    auto res = Image::Create(ctx, debugName, imageInfo);

    // If specific layout was provided - immediately transition the image:
    if (info.Layout.has_value())
    {
        ctx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
            auto barrierInfo = barrier::LayoutTransitionInfo{
                .Image            = res,
                .OldLayout        = VK_IMAGE_LAYOUT_UNDEFINED,
                .NewLayout        = *info.Layout,
            };

            barrier::ImageLayoutCoarse(cmd, barrierInfo);
        });
    }

    return res;
}

VkImageView MakeView::View2D(VulkanContext &ctx, const std::string &debugName, Image &img,
                             VkFormat format, VkImageAspectFlags aspectFlags)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

    viewInfo.image                       = img.Handle;
    viewInfo.format                      = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;

    // Hardcoded for now:
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = img.Info.mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;

    return Image::CreateView(ctx, debugName, viewInfo);
}

VkImageView MakeView::View2DArray(VulkanContext &ctx, const std::string &debugName,
                                  Image &img, VkFormat format,
                                  VkImageAspectFlags aspectFlags)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;

    viewInfo.image                       = img.Handle;
    viewInfo.format                      = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;

    // Hardcoded for now:
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = img.Info.mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = img.Info.arrayLayers;

    return Image::CreateView(ctx, debugName, viewInfo);
}

VkImageView MakeView::ViewCube(VulkanContext &ctx, const std::string &debugName,
                               Image &img, VkFormat format,
                               VkImageAspectFlags aspectFlags)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;

    viewInfo.image                       = img.Handle;
    viewInfo.format                      = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;

    // Hardcoded for now:
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = img.Info.mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 6;

    return Image::CreateView(ctx, debugName, viewInfo);
}

VkImageView MakeView::ViewArraySingleLayer(VulkanContext     &ctx,
                                           const std::string &debugName, Image &img,
                                           VkFormat           format,
                                           VkImageAspectFlags aspectFlags, uint32_t layer)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

    viewInfo.image                         = img.Handle;
    viewInfo.format                        = format;
    viewInfo.subresourceRange.aspectMask   = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount   = img.Info.mipLevels;

    // Hardcoded:
    viewInfo.subresourceRange.baseArrayLayer = layer;
    viewInfo.subresourceRange.layerCount     = 1;

    return Image::CreateView(ctx, debugName, viewInfo);
}

VkImageView MakeView::ViewCubeSingleMip(VulkanContext &ctx, const std::string &debugName,
                                        Image &img, VkFormat format,
                                        VkImageAspectFlags aspectFlags, uint32_t mip)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;

    viewInfo.image                         = img.Handle;
    viewInfo.format                        = format;
    viewInfo.subresourceRange.aspectMask   = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = mip;

    // Hardcoded:
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 6;

    return Image::CreateView(ctx, debugName, viewInfo);
}

Texture MakeTexture::Texture2D(VulkanContext &ctx, const std::string &debugName,
                               Image2DInfo info)
{
    Texture res;

    res.Img = MakeImage::Image2D(ctx, debugName, info);

    auto aspectMask = vkutils::GetDefaultAspect(info.Format);
    res.View        = MakeView::View2D(ctx, debugName, res.Img, info.Format, aspectMask);

    return res;
}

Texture MakeTexture::Texture2D(VulkanContext &ctx, const std::string &debugName,
                               Image2DInfo info, DeletionQueue &queue)
{
    Texture res = Texture2D(ctx, debugName, info);
    queue.push_back(res);
    return res;
}

Texture MakeTexture::Texture2DArray(VulkanContext &ctx, const std::string &debugName,
                                    Image2DInfo info, uint32_t numLayers)
{
    Texture res;

    res.Img = MakeImage::Image2DArray(ctx, debugName, info, numLayers);

    auto aspectMask = vkutils::GetDefaultAspect(info.Format);
    res.View = MakeView::View2DArray(ctx, debugName, res.Img, info.Format, aspectMask);

    return res;
}

Texture MakeTexture::Texture2DArray(VulkanContext &ctx, const std::string &debugName,
                                    Image2DInfo info, uint32_t numLayers,
                                    DeletionQueue &queue)
{
    Texture res = Texture2DArray(ctx, debugName, info, numLayers);
    queue.push_back(res);
    return res;
}

Texture MakeTexture::TextureCube(VulkanContext &ctx, const std::string &debugName,
                                 Image2DInfo info)
{
    Texture res;

    res.Img = MakeImage::Cube(ctx, debugName, info);

    auto aspectMask = vkutils::GetDefaultAspect(info.Format);
    res.View = MakeView::ViewCube(ctx, debugName, res.Img, info.Format, aspectMask);

    return res;
}

Texture MakeTexture::TextureCube(VulkanContext &ctx, const std::string &debugName,
                                 Image2DInfo info, DeletionQueue &queue)
{
    Texture res = TextureCube(ctx, debugName, info);
    queue.push_back(res);
    return res;
}