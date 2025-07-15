#include "Image.h"
#include "Pch.h"

#include "Barrier.h"
#include "Buffer.h"
#include "BufferUtils.h"
#include "VkUtils.h"

#include <vulkan/vulkan.h>

#include <libassert/assert.hpp>

#include <cmath>
#include <vulkan/vulkan_core.h>

uint32_t Image::CalcNumMips(uint32_t width, uint32_t height)
{
    return static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
}

Image Image::Create(VulkanContext &ctx, const std::string &debugName,
                    VkImageCreateInfo &info)
{
    Image img;
    img.Info = info;

    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    allocCreateInfo.priority = 1.0f;

    auto ret = vmaCreateImage(ctx.Allocator, &info, &allocCreateInfo, &img.Handle,
                              &img.Allocation, nullptr);

    ASSERT(ret == VK_SUCCESS, "Failed to create an image!");

    vkutils::SetDebugName(ctx, VK_OBJECT_TYPE_IMAGE, img.Handle, debugName);

    return img;
}

void Image::Destroy(VulkanContext &ctx, Image &img)
{
    vmaDestroyImage(ctx.Allocator, img.Handle, img.Allocation);
}

VkImageView Image::CreateView(VulkanContext &ctx, const std::string &debugName,
                              VkImageViewCreateInfo &info)
{
    VkImageView imageView;

    auto ret = vkCreateImageView(ctx.Device, &info, nullptr, &imageView);

    ASSERT(ret == VK_SUCCESS, "Failed to create image view!");

    vkutils::SetDebugName(ctx, VK_OBJECT_TYPE_IMAGE_VIEW, imageView, debugName);

    return imageView;
}

void Image::UploadToImage(VulkanContext &ctx, Image &img, ImageUploadInfo info)
{
    // Create buffer and upload image data
    Buffer stagingBuffer =
        MakeBuffer::Staging(ctx, "ImageUploadStagingBuffer", info.Size);
    Buffer::Upload(ctx, stagingBuffer, info.Data, info.Size);

    // Submit single-time command to queue
    ctx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
        // Change image layout to transfer destination:
        // Transitions all mip levels to dst layout
        auto barrierInfo = barrier::ImageLayoutBarrierInfo{
            .Image = img.Handle,
            .OldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .NewLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .SubresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, img.Info.mipLevels, 0, 1},
        };

        barrier::ImageLayoutBarrierCoarse(cmd, barrierInfo);

        // Copy buffer to image
        // Assumes that we are copying to entire image
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = img.Info.extent;
        // Assumes that image is color (not depth/stencil)
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        // Assumes that we are targeting mip level zero,
        // and that we have single image (not array)
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;

        vkCmdCopyBufferToImage(cmd, stagingBuffer.Handle, img.Handle,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        // Transition layout to whatever is needed
        barrierInfo.OldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrierInfo.NewLayout = info.DstLayout;

        barrier::ImageLayoutBarrierCoarse(cmd, barrierInfo);
    });

    Buffer::Destroy(ctx, stagingBuffer);
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
            auto srcInfo = barrier::ImageLayoutBarrierInfo{
                .Image = img.Handle,
                .OldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .NewLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .SubresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, mip - 1, 1, 0, numArrays},
            };

            barrier::ImageLayoutBarrierCoarse(cmd, srcInfo);

            auto dstInfo = barrier::ImageLayoutBarrierInfo{
                .Image = img.Handle,
                .OldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .NewLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .SubresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, mip, 1, 0, numArrays},
            };

            barrier::ImageLayoutBarrierCoarse(cmd, dstInfo);

            VkImageBlit2 blitRegion{};
            blitRegion.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;

            blitRegion.srcOffsets[1].x = srcSize.width;
            blitRegion.srcOffsets[1].y = srcSize.height;
            blitRegion.srcOffsets[1].z = 1;

            blitRegion.dstOffsets[1].x = dstSize.width;
            blitRegion.dstOffsets[1].y = dstSize.height;
            blitRegion.dstOffsets[1].z = 1;

            blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blitRegion.srcSubresource.baseArrayLayer = 0;
            blitRegion.srcSubresource.layerCount = numArrays;
            blitRegion.srcSubresource.mipLevel = mip - 1;

            blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blitRegion.dstSubresource.baseArrayLayer = 0;
            blitRegion.dstSubresource.layerCount = numArrays;
            blitRegion.dstSubresource.mipLevel = mip;

            VkBlitImageInfo2 blitInfo{};
            blitInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
            blitInfo.dstImage = img.Handle;
            blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            blitInfo.srcImage = img.Handle;
            blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            blitInfo.filter = VK_FILTER_LINEAR;
            blitInfo.regionCount = 1;
            blitInfo.pRegions = &blitRegion;

            vkCmdBlitImage2(cmd, &blitInfo);

            srcSize.width /= 2;
            srcSize.height /= 2;
            dstSize.width /= 2;
            dstSize.height /= 2;
        }

        auto finalInfo = barrier::ImageLayoutBarrierInfo{
            .Image = img.Handle,
            .OldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .NewLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .SubresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, img.Info.mipLevels, 0,
                                 img.Info.arrayLayers},
        };

        barrier::ImageLayoutBarrierCoarse(cmd, finalInfo);
    });
}
