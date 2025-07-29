#include "ImageUtils.h"
#include "Barrier.h"
#include "Pch.h"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

static VkExtent3D FromExtent2D(VkExtent2D extent)
{
    return VkExtent3D{
        .width = extent.width,
        .height = extent.height,
        .depth = 1,
    };
}

static bool IsDepthFormat(VkFormat format)
{
    //To-do: handle other depth formats:
    return format == VK_FORMAT_D32_SFLOAT;
}

Image MakeImage::Image2D(VulkanContext &ctx, const std::string &debugName,
                         Image2DInfo info)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = FromExtent2D(info.Extent);
    imageInfo.format = info.Format;
    imageInfo.usage = info.Usage;
    imageInfo.mipLevels = info.MipLevels;
    // This is actual order of pixels in memory, not sampler tiling:
    imageInfo.tiling = info.Tiling;

    // Hardcoded part:
    imageInfo.flags = 0;
    imageInfo.arrayLayers = 1;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    // Only other option is PREINITIALIZED:
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // Multisampling, only relevant for attachments:
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    auto res = Image::Create(ctx, debugName, imageInfo);

    if (info.Layout.has_value())
    {
        auto subresourceRange = VkImageSubresourceRange{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = res.Info.mipLevels,
            .baseArrayLayer = 0,
            .layerCount = res.Info.arrayLayers,
        };

        if (IsDepthFormat(info.Format))
            subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

        ctx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
            auto barrierInfo = barrier::ImageLayoutBarrierInfo{
                .Image = res.Handle,
                .OldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .NewLayout = *info.Layout,
                .SubresourceRange = subresourceRange,
            };

            barrier::ImageLayoutBarrierCoarse(cmd, barrierInfo);
        });
    }

    return res;
}

Image MakeImage::Cube(VulkanContext &ctx, const std::string &debugName, Image2DInfo info)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = FromExtent2D(info.Extent);
    imageInfo.format = info.Format;
    imageInfo.usage = info.Usage;
    imageInfo.mipLevels = info.MipLevels;
    imageInfo.tiling = info.Tiling; // Order of pixels in memory

    // Hardcoded part:
    imageInfo.arrayLayers = 6;
    imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    auto res = Image::Create(ctx, debugName, imageInfo);

    if (info.Layout.has_value())
    {
        ctx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
            auto barrierInfo = barrier::ImageLayoutBarrierInfo{
                .Image = res.Handle,
                .OldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .NewLayout = *info.Layout,
                .SubresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, res.Info.mipLevels, 0,
                                     res.Info.arrayLayers},
            };

            barrier::ImageLayoutBarrierCoarse(cmd, barrierInfo);
        });
    }

    return res;
}

VkImageView MakeView::View2D(VulkanContext &ctx, const std::string &debugName, Image &img,
                             VkFormat format, VkImageAspectFlags aspectFlags)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

    viewInfo.image = img.Handle;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;

    // Hardcoded for now:
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = img.Info.mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    return Image::CreateView(ctx, debugName, viewInfo);
}

VkImageView MakeView::ViewCube(VulkanContext &ctx, const std::string &debugName,
                               Image &img, VkFormat format,
                               VkImageAspectFlags aspectFlags)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;

    viewInfo.image = img.Handle;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;

    // Hardcoded for now:
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = img.Info.mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 6;

    return Image::CreateView(ctx, debugName, viewInfo);
}

VkImageView MakeView::ViewCubeSingleMip(VulkanContext &ctx, const std::string &debugName,
                                        Image &img, VkFormat format,
                                        VkImageAspectFlags aspectFlags, uint32_t mip)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;

    viewInfo.image = img.Handle;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = mip;

    // Hardcoded:
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 6;

    return Image::CreateView(ctx, debugName, viewInfo);
}

Texture MakeTexture::Texture2D(VulkanContext &ctx, const std::string &debugName,
                               Image2DInfo info)
{
    Texture res;

    res.Img = MakeImage::Image2D(ctx, debugName, info);
    res.View =
        MakeView::View2D(ctx, debugName, res.Img, info.Format, VK_IMAGE_ASPECT_COLOR_BIT);

    return res;
}

Texture MakeTexture::Texture2D(VulkanContext &ctx, const std::string &debugName,
                               Image2DInfo info, DeletionQueue &queue)
{
    auto res = Texture2D(ctx, debugName, info);
    queue.push_back(res);
    return res;
}

Texture MakeTexture::TextureCube(VulkanContext &ctx, const std::string &debugName,
                                 Image2DInfo info)
{
    Texture res;

    res.Img = MakeImage::Cube(ctx, debugName, info);
    res.View = MakeView::ViewCube(ctx, debugName, res.Img, info.Format,
                                  VK_IMAGE_ASPECT_COLOR_BIT);

    return res;
}

Texture MakeTexture::TextureCube(VulkanContext &ctx, const std::string &debugName,
                                 Image2DInfo info, DeletionQueue &queue)
{
    auto res = TextureCube(ctx, debugName, info);
    queue.push_back(res);
    return res;
}