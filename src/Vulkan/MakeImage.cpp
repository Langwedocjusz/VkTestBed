#include "MakeImage.h"
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

static VkExtent2D GetExtent2D(const ImageData &data)
{
    return VkExtent2D{
        .width  = static_cast<uint32_t>(data.Width),
        .height = static_cast<uint32_t>(data.Height),
    };
}

static VkImageCreateInfo GetVkCreateImageInfo(const Image2DInfo &info)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType   = VK_IMAGE_TYPE_2D;
    imageInfo.extent      = Extent2DTo3D(info.Extent);
    imageInfo.format      = info.Format;
    imageInfo.usage       = info.Usage;
    imageInfo.mipLevels   = info.MipLevels;
    imageInfo.samples     = info.Multisampling;
    imageInfo.arrayLayers = 1;

    // Hardcoded for now, may have to change when
    // async-compute/transfer queues are supported:
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    // This is actual order of pixels in memory, not sampler tiling:
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    // Only other option is PREINITIALIZED:
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    return imageInfo;
}

static void HandleLayoutTransition(VulkanContext &ctx, Image &img,
                                   std::optional<VkImageLayout> layout)
{
    // If specific layout was provided - immediately transition the image:
    if (layout.has_value())
    {
        ctx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
            auto barrierInfo = barrier::LayoutTransitionInfo{
                .Image     = img,
                .OldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .NewLayout = *layout,
            };

            barrier::ImageLayoutCoarse(cmd, barrierInfo);
        });
    }
}

Image MakeImage::Image2D(VulkanContext &ctx, const std::string &debugName,
                         Image2DInfo info)
{
    auto imageInfo = GetVkCreateImageInfo(info);

    auto res = Image::Create(ctx, debugName, imageInfo);
    HandleLayoutTransition(ctx, res, info.Layout);

    return res;
}

Image MakeImage::Image2DArray(VulkanContext &ctx, const std::string &debugName,
                              Image2DInfo info, uint32_t numLayers)
{
    auto imageInfo        = GetVkCreateImageInfo(info);
    imageInfo.arrayLayers = numLayers;

    auto res = Image::Create(ctx, debugName, imageInfo);
    HandleLayoutTransition(ctx, res, info.Layout);

    return res;
}

Image MakeImage::Cube(VulkanContext &ctx, const std::string &debugName, Image2DInfo info)
{
    auto imageInfo        = GetVkCreateImageInfo(info);
    imageInfo.arrayLayers = 6;
    imageInfo.flags       = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    auto res = Image::Create(ctx, debugName, imageInfo);
    HandleLayoutTransition(ctx, res, info.Layout);

    return res;
}

Image MakeImage::FromData(VulkanContext &ctx, const std::string &debugName,
                          const ImageData &data)
{
    auto extent = GetExtent2D(data);

    Image2DInfo imgInfo{
        .Extent = extent,
        .Format = data.Format,
        .Usage  = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    };

    if (data.Mips == MipStrategy::Load)
        imgInfo.MipLevels = data.NumMips;

    if (data.Mips == MipStrategy::Generate)
    {
        imgInfo.MipLevels = Image::CalcNumMips(data.Width, data.Height);
        imgInfo.Usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    Image img = MakeImage::Image2D(ctx, debugName, imgInfo);

    Image::UploadInfo uploadInfo{
        .Data       = data.Data,
        .Size       = data.Size,
        .DstLayout  = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .MipOffsets = {},
    };

    if (data.Mips == MipStrategy::Load)
    {
        uploadInfo.AllMips    = true;
        uploadInfo.MipOffsets = data.MipOffsets;
    }

    Image::UploadToImage(ctx, img, uploadInfo);

    if (data.Mips == MipStrategy::Generate)
        Image::GenerateMips(ctx, img);

    return img;
}

static VkImageViewCreateInfo GetVkCreateImageViewInfo(View2DInfo info)
{
    VkImageViewCreateInfo ret{};
    ret.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;

    ret.image            = info.Img.Handle;
    ret.format           = info.Img.Info.format;
    ret.subresourceRange = Image::GetDefaultRange(info.Img);

    if (info.SelectLevel.has_value())
    {
        ret.subresourceRange.baseMipLevel = *info.SelectLevel;
        ret.subresourceRange.levelCount   = 1;
    }

    if (info.SelectLayer.has_value())
    {
        ret.subresourceRange.baseArrayLayer = *info.SelectLayer;
        ret.subresourceRange.layerCount     = 1;
    }

    if (info.FormatOverride.has_value())
    {
        ret.format                      = *info.FormatOverride;
        ret.subresourceRange.aspectMask = vkutils::GetDefaultAspect(ret.format);
    }

    if (info.AspectOverride.has_value())
    {
        ret.subresourceRange.aspectMask = *info.AspectOverride;
    }

    return ret;
}

VkImageView MakeView::View2D(VulkanContext &ctx, const std::string &debugName,
                             View2DInfo info)
{
    auto viewInfo     = GetVkCreateImageViewInfo(info);
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

    return Image::CreateView(ctx, debugName, viewInfo);
}

VkImageView MakeView::View2DArray(VulkanContext &ctx, const std::string &debugName,
                                  View2DInfo info)
{
    auto viewInfo     = GetVkCreateImageViewInfo(info);
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;

    return Image::CreateView(ctx, debugName, viewInfo);
}

VkImageView MakeView::ViewCube(VulkanContext &ctx, const std::string &debugName,
                               View2DInfo info)
{
    auto viewInfo     = GetVkCreateImageViewInfo(info);
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;

    return Image::CreateView(ctx, debugName, viewInfo);
}

// Texture-wrapped functions:

Texture MakeTexture::Texture2D(VulkanContext &ctx, const std::string &debugName,
                               Image2DInfo info)
{
    Texture res;
    res.Img  = MakeImage::Image2D(ctx, debugName, info);
    res.View = MakeView::View2D(ctx, debugName, {.Img = res.Img});

    return res;
}

Texture MakeTexture::Texture2DArray(VulkanContext &ctx, const std::string &debugName,
                                    Image2DInfo info, uint32_t numLayers)
{
    Texture res;
    res.Img  = MakeImage::Image2DArray(ctx, debugName, info, numLayers);
    res.View = MakeView::View2DArray(ctx, debugName, {.Img = res.Img});

    return res;
}

Texture MakeTexture::TextureCube(VulkanContext &ctx, const std::string &debugName,
                                 Image2DInfo info)
{
    Texture res;
    res.Img  = MakeImage::Cube(ctx, debugName, info);
    res.View = MakeView::ViewCube(ctx, debugName, {.Img = res.Img});

    return res;
}

Texture MakeTexture::FromData(VulkanContext &ctx, const std::string &debugName,
                              const ImageData &data)
{
    Texture res{};
    res.Img  = MakeImage::FromData(ctx, debugName, data);
    res.View = MakeView::View2D(ctx, debugName, {.Img = res.Img});

    return res;
}

// Deletion queue hooked overloads:

Texture MakeTexture::Texture2D(VulkanContext &ctx, const std::string &debugName,
                               Image2DInfo info, DeletionQueue &queue)
{
    Texture res = Texture2D(ctx, debugName, info);
    queue.push_back(res);
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
                                 Image2DInfo info, DeletionQueue &queue)
{
    Texture res = TextureCube(ctx, debugName, info);
    queue.push_back(res);
    return res;
}

Texture MakeTexture::FromData(VulkanContext &ctx, const std::string &debugName,
                              const ImageData &data, DeletionQueue &queue)
{
    Texture res = FromData(ctx, debugName, data);
    queue.push_back(res);
    return res;
}