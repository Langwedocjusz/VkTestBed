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

Image::Image(VulkanContext &ctx, const Image2DInfo &info, uint32_t numLayers, VkImageCreateFlags flags, InternalType type)
    : mCtx(ctx), mType(type)
{
    Info = {};
    Info.sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    Info.imageType   = VK_IMAGE_TYPE_2D;
    Info.extent      = Extent2DTo3D(info.Extent);
    Info.format      = info.Format;
    Info.usage       = info.Usage;
    Info.mipLevels   = info.MipLevels;
    Info.samples     = info.Multisampling;
    Info.arrayLayers = numLayers;
    Info.flags       = flags;

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
    DebugName = info.Name;

    // If specific layout was provided - immediately transition the image:
    if (info.Layout.has_value())
    {
        ctx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
            auto barrierInfo = barrier::LayoutTransitionInfo{
                .Image     = *this,
                .OldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .NewLayout = *info.Layout,
            };

            barrier::ImageLayoutCoarse(cmd, barrierInfo);
        });
    }
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

Image& Image::operator=(Image&& other) noexcept
{
    if (this != &other)
    {
        std::swap(Handle, other.Handle);
        std::swap(Info, other.Info);
        std::swap(Allocation, other.Allocation);
        std::swap(DebugName, other.DebugName);
        std::swap(mType, other.mType);
    }

    return *this;
}

// === Factory functions: ====================================================

Image Image::Make2D(VulkanContext &ctx, const Image2DInfo &info)
{
    return Image{ctx, info, 1, 0, InternalType::Single2D};
}

Image Image::Make2DArray(VulkanContext &ctx, const Image2DInfo &info, uint32_t numLayers)
{
    return Image{ctx, info, numLayers, 0, InternalType::Single2D};
}

Image Image::MakeCube(VulkanContext &ctx, const Image2DInfo &info)
{
    return Image{ctx, info, 6, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, InternalType::Cube};
}

Image Image::FromData(VulkanContext &ctx, const std::string &debugName,
                      const ImageData &data)
{
    VkExtent2D extent{
        .width  = static_cast<uint32_t>(data.Width),
        .height = static_cast<uint32_t>(data.Height),
    };

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
        uploadInfo.MipOffsets = data.MipOffsets;
    }

    img.UploadData(uploadInfo);

    if (data.Mips == MipStrategy::Generate)
        img.GenerateMips();

    return img;
}

// === Creating Image Views: ===================================================

static VkImageViewCreateInfo ParseViewInfo(Image &img, View2DInfo info)
{
    VkImageViewCreateInfo ret{};
    ret.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;

    ret.image            = img.Handle;
    ret.format           = img.Info.format;
    ret.subresourceRange = img.GetDefaultRange();

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

VkImageView Image::MakeView(const View2DInfo &info)
{
    using enum Image::InternalType;

    auto viewInfo = ParseViewInfo(*this, info);

    switch (mType)
    {
        case Single2D: {
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            break;
        }
        case Array2D: {
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            break;
        }
        case Cube: {
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
            break;
        }
        case None: {
            vpanic("Trying to create view from unsupported image type!");
        }
    }

    VkImageView ret{};

    auto result = vkCreateImageView(mCtx.Device, &viewInfo, nullptr, &ret);

    vassert(result == VK_SUCCESS, "Failed to create image view!");

    vkutils::SetDebugName(mCtx, VK_OBJECT_TYPE_IMAGE_VIEW, ret, info.Name);

    return ret;
}

// === Other member functions: =============================================

void Image::UploadData(const Image::UploadInfo &info)
{
    // Sanity checks - we only support singular 2D images atm:
    vassert(Info.extent.depth == 1);
    vassert(Info.arrayLayers == 1);

    // Create buffer and upload image data
    Buffer::ThinInfo stagingInfo{
        .Name = "ImageUploadStagingBuffer",
        .Size = info.Size,
    };
    Buffer stagingBuffer = Buffer::MakeStaging(mCtx, stagingInfo);

    stagingBuffer.Upload(info.Data, info.Size);

    // Submit single-time command to queue
    mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
        // Change image layout to transfer destination:
        // Transitions all mip levels to dst layout
        auto barrierInfo = barrier::LayoutTransitionInfo{
            .Image     = *this,
            .OldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .NewLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        };

        barrier::ImageLayoutCoarse(cmd, barrierInfo);

        if (info.MipOffsets.empty())
        {
            // No mip offsets provided. Assume single copy region targeting mip 0:
            VkBufferImageCopy region{};

            region.imageExtent               = Info.extent;
            region.imageSubresource.mipLevel = 0;
            region.bufferOffset              = 0;

            region.bufferRowLength   = 0;
            region.bufferImageHeight = 0;
            region.imageOffset       = {0, 0, 0};
            // Assumes that image is color (not depth/stencil):
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.baseArrayLayer = 0;
            region.imageSubresource.layerCount     = 1;

            vkCmdCopyBufferToImage(cmd, stagingBuffer.Handle, Handle,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        }
        else
        {
            // Multiple copy regions - one per mip level:
            std::vector<VkBufferImageCopy> regions{};

            for (uint32_t lvl = 0; lvl < Info.mipLevels; lvl++)
            {
                VkBufferImageCopy region{};

                auto currentOffset = info.MipOffsets[lvl];

                region.imageSubresource.mipLevel = lvl;
                region.bufferOffset              = currentOffset;

                auto width  = Info.extent.width >> lvl;
                auto height = Info.extent.height >> lvl;

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

            vkCmdCopyBufferToImage(cmd, stagingBuffer.Handle, Handle,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   static_cast<uint32_t>(regions.size()), regions.data());
        }

        // Transition layout to whatever is needed
        barrierInfo.OldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrierInfo.NewLayout = info.DstLayout;

        barrier::ImageLayoutCoarse(cmd, barrierInfo);
    });
}



void Image::GenerateMips()
{
    mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
        VkExtent3D srcSize = Info.extent;
        VkExtent3D dstSize = Info.extent;

        const auto numArrays = Info.arrayLayers;

        dstSize.width /= 2;
        dstSize.height /= 2;

        for (uint32_t mip = 1; mip < Info.mipLevels; mip++)
        {
            auto srcInfo = barrier::LayoutTransitionInfo{
                .Image            = *this,
                .OldLayout        = VK_IMAGE_LAYOUT_UNDEFINED,
                .NewLayout        = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .SubresourceRange = VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT,
                                                            mip - 1, 1, 0, numArrays},
            };

            barrier::ImageLayoutCoarse(cmd, srcInfo);

            auto dstInfo = barrier::LayoutTransitionInfo{
                .Image            = *this,
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
            blitInfo.dstImage       = Handle;
            blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            blitInfo.srcImage       = Handle;
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
            .Image     = *this,
            .OldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .NewLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };

        barrier::ImageLayoutCoarse(cmd, finalInfo);
    });
}

VkImageSubresourceRange Image::GetDefaultRange() const
{
    VkImageSubresourceRange range{
        .aspectMask     = vkutils::GetDefaultAspect(Info.format),
        .baseMipLevel   = 0,
        .levelCount     = Info.mipLevels,
        .baseArrayLayer = 0,
        .layerCount     = Info.arrayLayers,
    };

    return range;
}

// === Static helpers: =======================================================

uint32_t Image::MipCount(uint32_t size)
{
    return static_cast<uint32_t>(std::floor(std::log2(size))) + 1;
}

uint32_t Image::MipCount(uint32_t width, uint32_t height)
{
    return MipCount(std::max(width, height));
}


// === Texture implementation: ================================================

Texture::Texture(VulkanContext &ctx, Image &&img)
    : Img(std::move(img)), mCtx(ctx)
{
    View2DInfo info{
        .Name = img.DebugName + "View",
    };

    View = Img.MakeView(info);
}

Texture::~Texture()
{
    vkDestroyImageView(mCtx.Device, View, nullptr);
}

Texture::Texture(Texture &&other) noexcept
    : Img(std::move(other.Img))
    , View(other.View)
    , mCtx(other.mCtx)
{
    other.View = {};
}

Texture& Texture::operator=(Texture&& other) noexcept
{
    if (this != &other)
    {
        std::swap(Img, other.Img);
        std::swap(View, other.View);
    }

    return *this;
}

Texture Texture::Make2D(VulkanContext &ctx, const Image2DInfo &info)
{
    return Texture{ctx, Image::Make2D(ctx, info)};
}

Texture Texture::Make2DArray(VulkanContext &ctx, const Image2DInfo &info,
                             uint32_t numLayers)
{
    return Texture{ctx, Image::Make2DArray(ctx, info, numLayers)};
}

Texture Texture::MakeCube(VulkanContext &ctx, const Image2DInfo &info)
{
    return Texture{ctx, Image::MakeCube(ctx, info)};
}

Texture Texture::FromData(VulkanContext &ctx, const std::string &debugName,
                          const ImageData &data)
{
    return Texture{ctx, Image::FromData(ctx, debugName, data)};
}