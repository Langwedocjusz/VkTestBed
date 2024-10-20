#include "Image.h"

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