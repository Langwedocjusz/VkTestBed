#include "Image.h"

#include "Barrier.h"
#include "Buffer.h"
#include "DeletionQueue.h"
#include "Utils.h"
#include <vulkan/vulkan_core.h>

Image Image::CreateImage2D(VulkanContext &ctx, ImageInfo info)
{
    Image img;
    img.Info = info;

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;

    assert(info.Extent.depth == 1);

    imageInfo.extent = info.Extent;
    imageInfo.format = info.Format;
    imageInfo.usage = info.Usage;
    // This is actual order of pixels in memory, not sampler tiling:
    imageInfo.tiling = info.Tiling;

    // Hardcoded part:
    imageInfo.flags = 0;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    // Only other option is PREINITIALIZED:
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // Multisampling, only relevant for attachments:
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    allocCreateInfo.priority = 1.0f;

    vmaCreateImage(ctx.Allocator, &imageInfo, &allocCreateInfo, &img.Handle,
                   &img.Allocation, nullptr);

    return img;
}

Image Image::CreateCubemap(VulkanContext &ctx, ImageInfo info)
{
    Image img;
    img.Info = info;

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;

    assert(info.Extent.depth == 1);

    imageInfo.extent = info.Extent;
    imageInfo.format = info.Format;
    imageInfo.usage = info.Usage;
    imageInfo.tiling = info.Tiling; // Order of pixels in memory

    // Hardcoded part:
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 6;
    imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    allocCreateInfo.priority = 1.0f;

    vmaCreateImage(ctx.Allocator, &imageInfo, &allocCreateInfo, &img.Handle,
                   &img.Allocation, nullptr);

    return img;
}

void Image::DestroyImage(VulkanContext &ctx, Image &img)
{
    vmaDestroyImage(ctx.Allocator, img.Handle, img.Allocation);
}

VkImageView Image::CreateView2D(VulkanContext &ctx, Image &img, VkFormat format,
                                VkImageAspectFlags aspectFlags)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

    viewInfo.image = img.Handle;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;

    // Hardcoded for now:
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;

    if (vkCreateImageView(ctx.Device, &viewInfo, nullptr, &imageView) != VK_SUCCESS)
        throw std::runtime_error("Failed to create texture image view!");

    return imageView;
}

VkImageView Image::CreateViewCube(VulkanContext &ctx, Image &img, VkFormat format,
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
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 6;

    VkImageView imageView;

    if (vkCreateImageView(ctx.Device, &viewInfo, nullptr, &imageView) != VK_SUCCESS)
        throw std::runtime_error("Failed to create texture image view!");

    return imageView;
}

void Image::UploadToImage(VulkanContext &ctx, Image img, ImageUploadInfo info)
{
    // Create buffer and upload image data
    Buffer stagingBuffer = Buffer::CreateStagingBuffer(ctx, info.Size);
    Buffer::UploadToBuffer(ctx, stagingBuffer, info.Data, info.Size);

    // Submit single-time command to queue
    {
        utils::ScopedCommand cmd(ctx, info.Queue, info.Pool);

        // Change image layout to transfer destination:
        auto barrierInfo = barrier::ImageLayoutBarrierInfo{
            .Image = img.Handle,
            .OldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .NewLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .SubresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        };

        barrier::ImageLayoutBarrierCoarse(cmd.Buffer, barrierInfo);

        // Copy buffer to image
        // Assumes that we are copying to entire image
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = img.Info.Extent;
        // Assumes that image is color (not depth/stencil)
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        // Assumes that we are targeting mip level zero,
        // and that we have single image (not array)
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;

        vkCmdCopyBufferToImage(cmd.Buffer, stagingBuffer.Handle, img.Handle,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        // Transition layout to whatever is needed
        barrierInfo.OldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrierInfo.NewLayout = info.DstLayout;

        barrier::ImageLayoutBarrierCoarse(cmd.Buffer, barrierInfo);
    }

    Buffer::DestroyBuffer(ctx, stagingBuffer);
}