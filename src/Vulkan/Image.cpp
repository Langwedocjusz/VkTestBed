#include "Image.h"
#include "Pch.h"

#include "Barrier.h"
#include "Buffer.h"
#include "Vassert.h"
#include "VkUtils.h"

#include "volk.h"

#include <cmath>

static VkExtent3D Extent2DTo3D(VkExtent2D extent)
{
    return VkExtent3D{
        .width  = extent.width,
        .height = extent.height,
        .depth  = 1,
    };
}

Image::Image(VulkanContext &ctx, const Image2DInfo &info)
    : mCtx(ctx)
{
    Info = {};
    Info.sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    Info.imageType   = VK_IMAGE_TYPE_2D;
    Info.extent      = Extent2DTo3D(info.Extent);
    Info.format      = info.Format;
    Info.usage       = info.Usage;
    Info.mipLevels   = info.MipLevels;
    Info.samples     = info.Multisampling;
    Info.arrayLayers = 1;

    // Hardcoded for now, may have to change when
    // async-compute/transfer queues are supported:
    Info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    // This is actual order of pixels in memory, not sampler tiling:
    Info.tiling = VK_IMAGE_TILING_OPTIMAL;
    // Only other option is PREINITIALIZED:
    Info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.usage                   = VMA_MEMORY_USAGE_AUTO;
    allocCreateInfo.flags                   = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    allocCreateInfo.priority                = 1.0f;

    auto ret = vmaCreateImage(ctx.Allocator, &Info, &allocCreateInfo, &Handle,
                              &Allocation, nullptr);

    vassert(ret == VK_SUCCESS, "Failed to create an image!");

    vkutils::SetDebugName(ctx, VK_OBJECT_TYPE_IMAGE, Handle, info.Name);
}

Image::~Image()
{
    vmaDestroyImage(mCtx.Allocator, Handle, Allocation);
}

Image::Image(Image &&other) noexcept
    : Handle(other.Handle)
    , Info(other.Info)
    , Allocation(other.Allocation)
    , mCtx(other.mCtx)
{
    other.Handle = {};
    other.Info = {};
    other.Allocation = {};
}

VkImageView Image::CreateView(VulkanContext &ctx, const std::string &debugName,
                              VkImageViewCreateInfo &info)
{
    VkImageView imageView;

    auto ret = vkCreateImageView(ctx.Device, &info, nullptr, &imageView);

    vassert(ret == VK_SUCCESS, "Failed to create image view!");

    vkutils::SetDebugName(ctx, VK_OBJECT_TYPE_IMAGE_VIEW, imageView, debugName);

    return imageView;
}

void Image::UploadToImage(VulkanContext &ctx, Image &img, Image::UploadInfo info)
{
    // Sanity checks - we only support singular 2D images atm:
    vassert(img.Info.extent.depth == 1);
    vassert(img.Info.arrayLayers == 1);

    // Create buffer and upload image data
    Buffer::ThinInfo stagingInfo{
        .Name = "ImageUploadStagingBuffer",
        .Size = info.Size,
    };
    Buffer stagingBuffer = Buffer::MakeStaging(ctx, stagingInfo);

    stagingBuffer.Upload(info.Data, info.Size);

    // Submit single-time command to queue
    ctx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
        // Change image layout to transfer destination:
        // Transitions all mip levels to dst layout
        auto barrierInfo = barrier::LayoutTransitionInfo{
            .Image     = img,
            .OldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .NewLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        };

        barrier::ImageLayoutCoarse(cmd, barrierInfo);

        if (info.AllMips)
        {
            // Multiple copy regions - one per mip level:
            std::vector<VkBufferImageCopy> regions{};

            for (uint32_t lvl = 0; lvl < img.Info.mipLevels; lvl++)
            {
                VkBufferImageCopy region{};

                auto currentOffset = info.MipOffsets[lvl];

                region.imageSubresource.mipLevel = lvl;
                region.bufferOffset              = currentOffset;

                auto width  = img.Info.extent.width >> lvl;
                auto height = img.Info.extent.height >> lvl;

                region.imageExtent.width  = width;
                region.imageExtent.height = height;
                region.imageExtent.depth  = 1;

                region.bufferRowLength   = 0;
                region.bufferImageHeight = 0;
                region.imageOffset       = {0, 0, 0};
                // Assumes that image is color (not depth/stencil)
                region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                // Assumes that we have single image (not array)
                region.imageSubresource.baseArrayLayer = 0;
                region.imageSubresource.layerCount     = 1;

                regions.push_back(region);
            }

            vkCmdCopyBufferToImage(cmd, stagingBuffer.Handle, img.Handle,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   static_cast<uint32_t>(regions.size()), regions.data());
        }
        else
        {
            // Single copy region targeting mip 0:
            VkBufferImageCopy region{};

            region.imageExtent               = img.Info.extent;
            region.imageSubresource.mipLevel = 0;
            region.bufferOffset              = 0;

            region.bufferRowLength   = 0;
            region.bufferImageHeight = 0;
            region.imageOffset       = {0, 0, 0};
            // Assumes that image is color (not depth/stencil)
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            // Assumes that we have single image (not array)
            region.imageSubresource.baseArrayLayer = 0;
            region.imageSubresource.layerCount     = 1;

            vkCmdCopyBufferToImage(cmd, stagingBuffer.Handle, img.Handle,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        }

        // Transition layout to whatever is needed
        barrierInfo.OldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrierInfo.NewLayout = info.DstLayout;

        barrier::ImageLayoutCoarse(cmd, barrierInfo);
    });
}

uint32_t Image::MipCount(uint32_t size)
{
    return static_cast<uint32_t>(std::floor(std::log2(size))) + 1;
}

uint32_t Image::MipCount(uint32_t width, uint32_t height)
{
    return MipCount(std::max(width, height));
}

void Image::GenerateMips(VulkanContext &ctx, Image &img)
{
    ctx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
        VkExtent3D srcSize = img.Info.extent;
        VkExtent3D dstSize = img.Info.extent;

        const auto numArrays = img.Info.arrayLayers;

        dstSize.width /= 2;
        dstSize.height /= 2;

        for (uint32_t mip = 1; mip < img.Info.mipLevels; mip++)
        {
            auto srcInfo = barrier::LayoutTransitionInfo{
                .Image            = img,
                .OldLayout        = VK_IMAGE_LAYOUT_UNDEFINED,
                .NewLayout        = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .SubresourceRange = VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT,
                                                            mip - 1, 1, 0, numArrays},
            };

            barrier::ImageLayoutCoarse(cmd, srcInfo);

            auto dstInfo = barrier::LayoutTransitionInfo{
                .Image            = img,
                .OldLayout        = VK_IMAGE_LAYOUT_UNDEFINED,
                .NewLayout        = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .SubresourceRange = VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT,
                                                            mip, 1, 0, numArrays},
            };

            barrier::ImageLayoutCoarse(cmd, dstInfo);

            VkImageBlit2 blitRegion{};
            blitRegion.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;

            blitRegion.srcOffsets[1].x = srcSize.width;
            blitRegion.srcOffsets[1].y = srcSize.height;
            blitRegion.srcOffsets[1].z = 1;

            blitRegion.dstOffsets[1].x = dstSize.width;
            blitRegion.dstOffsets[1].y = dstSize.height;
            blitRegion.dstOffsets[1].z = 1;

            blitRegion.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            blitRegion.srcSubresource.baseArrayLayer = 0;
            blitRegion.srcSubresource.layerCount     = numArrays;
            blitRegion.srcSubresource.mipLevel       = mip - 1;

            blitRegion.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            blitRegion.dstSubresource.baseArrayLayer = 0;
            blitRegion.dstSubresource.layerCount     = numArrays;
            blitRegion.dstSubresource.mipLevel       = mip;

            VkBlitImageInfo2 blitInfo{};
            blitInfo.sType          = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
            blitInfo.dstImage       = img.Handle;
            blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            blitInfo.srcImage       = img.Handle;
            blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            blitInfo.filter         = VK_FILTER_LINEAR;
            blitInfo.regionCount    = 1;
            blitInfo.pRegions       = &blitRegion;

            vkCmdBlitImage2(cmd, &blitInfo);

            srcSize.width /= 2;
            srcSize.height /= 2;
            dstSize.width /= 2;
            dstSize.height /= 2;
        }

        auto finalInfo = barrier::LayoutTransitionInfo{
            .Image     = img,
            .OldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .NewLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };

        barrier::ImageLayoutCoarse(cmd, finalInfo);
    });
}

VkImageSubresourceRange Image::GetDefaultRange(const Image &img)
{
    VkImageSubresourceRange range{
        .aspectMask     = vkutils::GetDefaultAspect(img.Info.format),
        .baseMipLevel   = 0,
        .levelCount     = img.Info.mipLevels,
        .baseArrayLayer = 0,
        .layerCount     = img.Info.arrayLayers,
    };

    return range;
}



static VkExtent2D GetExtent2D(const ImageData &data)
{
    return VkExtent2D{
        .width  = static_cast<uint32_t>(data.Width),
        .height = static_cast<uint32_t>(data.Height),
    };
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

Image Image::Make2D(VulkanContext &ctx, const Image2DInfo &info)
{
    auto imageInfo = GetVkCreateImageInfo(info);

    auto res = Image::Create(ctx, info.Name, imageInfo);
    HandleLayoutTransition(ctx, res, info.Layout);

    return res;
}

Image Image::Make2DArray(VulkanContext &ctx, const Image2DInfo &info, uint32_t numLayers)
{
    auto imageInfo        = GetVkCreateImageInfo(info);
    imageInfo.arrayLayers = numLayers;

    auto res = Image::Create(ctx, info.Name, imageInfo);
    HandleLayoutTransition(ctx, res, info.Layout);

    return res;
}

Image Image::MakeCube(VulkanContext &ctx, const Image2DInfo &info)
{
    auto imageInfo        = GetVkCreateImageInfo(info);
    imageInfo.arrayLayers = 6;
    imageInfo.flags       = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    auto res = Image::Create(ctx, info.Name, imageInfo);
    HandleLayoutTransition(ctx, res, info.Layout);

    return res;
}

Image Image::FromData(VulkanContext &ctx, const std::string &debugName,
                      const ImageData &data)
{
    auto extent = GetExtent2D(data);

    Image2DInfo imgInfo{
        // NOTE: String copy:
        .Name   = debugName,
        .Extent = extent,
        .Format = data.Format,
        .Usage  = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    };

    if (data.Mips == MipStrategy::Load)
        imgInfo.MipLevels = data.NumMips;

    if (data.Mips == MipStrategy::Generate)
    {
        imgInfo.MipLevels = Image::MipCount(data.Width, data.Height);
        imgInfo.Usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    Image img = Make2D(ctx, imgInfo);

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

VkImageView Image::MakeView2D(VulkanContext &ctx, const std::string &debugName,
                              View2DInfo info)
{
    auto viewInfo     = GetVkCreateImageViewInfo(info);
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

    return Image::CreateView(ctx, debugName, viewInfo);
}

VkImageView Image::MakeView2DArray(VulkanContext &ctx, const std::string &debugName,
                                   View2DInfo info)
{
    auto viewInfo     = GetVkCreateImageViewInfo(info);
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;

    return Image::CreateView(ctx, debugName, viewInfo);
}

VkImageView Image::MakeViewCube(VulkanContext &ctx, const std::string &debugName,
                                View2DInfo info)
{
    auto viewInfo     = GetVkCreateImageViewInfo(info);
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;

    return Image::CreateView(ctx, debugName, viewInfo);
}

// Texture-wrapped functions:

Texture Texture::Make2D(VulkanContext &ctx, const Image2DInfo &info)
{
    Texture res;
    res.Img  = Image::Make2D(ctx, info);
    res.View = Image::MakeView2D(ctx, info.Name, {.Img = res.Img});

    return res;
}

Texture Texture::Make2DArray(VulkanContext &ctx, const Image2DInfo &info,
                             uint32_t numLayers)
{
    Texture res;
    res.Img  = Image::Make2DArray(ctx, info, numLayers);
    res.View = Image::MakeView2DArray(ctx, info.Name, {.Img = res.Img});

    return res;
}

Texture Texture::MakeCube(VulkanContext &ctx, const Image2DInfo &info)
{
    Texture res;
    res.Img  = Image::MakeCube(ctx, info);
    res.View = Image::MakeViewCube(ctx, info.Name, {.Img = res.Img});

    return res;
}

Texture Texture::FromData(VulkanContext &ctx, const std::string &debugName,
                          const ImageData &data)
{
    Texture res{};
    res.Img  = Image::FromData(ctx, debugName, data);
    res.View = Image::MakeView2D(ctx, debugName, {.Img = res.Img});

    return res;
}

// Deletion queue hooked overloads:

Texture Texture::Make2D(VulkanContext &ctx, const Image2DInfo &info, DeletionQueue &queue)
{
    Texture res = Make2D(ctx, info);
    queue.push_back(res);
    return res;
}

Texture Texture::Make2DArray(VulkanContext &ctx, const Image2DInfo &info,
                             uint32_t numLayers, DeletionQueue &queue)
{
    Texture res = Make2DArray(ctx, info, numLayers);
    queue.push_back(res);
    return res;
}

Texture Texture::MakeCube(VulkanContext &ctx, const Image2DInfo &info,
                          DeletionQueue &queue)
{
    Texture res = MakeCube(ctx, info);
    queue.push_back(res);
    return res;
}

Texture Texture::FromData(VulkanContext &ctx, const std::string &debugName,
                          const ImageData &data, DeletionQueue &queue)
{
    Texture res = FromData(ctx, debugName, data);
    queue.push_back(res);
    return res;
}