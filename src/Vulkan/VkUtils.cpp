#include "VkUtils.h"
#include "Pch.h"

#include "Vassert.h"

#include <algorithm>

VkImageAspectFlags vkutils::GetDefaultAspect(VkFormat format)
{
    using std::ranges::contains;

    // Check if contains both depth and stencil:
    static constexpr std::array depthStencilFormats{
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM_S8_UINT,
    };
    if (contains(depthStencilFormats,format))
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

    // Otherwise check if depth only:
    static constexpr std::array depthFormats{
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D16_UNORM,
    };
    if (contains(depthFormats, format))
        return VK_IMAGE_ASPECT_DEPTH_BIT;

    // Otherwise check if stencil only:
    static constexpr std::array stencilFormats{
        VK_FORMAT_S8_UINT,
    };
    if (contains(stencilFormats, format))
        return VK_IMAGE_ASPECT_STENCIL_BIT;

    // Otherwise assume color:
    return VK_IMAGE_ASPECT_COLOR_BIT;
}

bool vkutils::IsFormatSRGB(VkFormat format)
{
    using std::ranges::contains;

    static constexpr std::array srgbFormats{
        VK_FORMAT_R8_SRGB,
        VK_FORMAT_R8G8_SRGB,
        VK_FORMAT_R8G8B8_SRGB,
        VK_FORMAT_B8G8R8_SRGB,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_B8G8R8A8_SRGB,
        VK_FORMAT_A8B8G8R8_SRGB_PACK32,
        VK_FORMAT_BC1_RGB_SRGB_BLOCK,
        VK_FORMAT_BC1_RGBA_SRGB_BLOCK,
        VK_FORMAT_BC2_SRGB_BLOCK,
        VK_FORMAT_BC3_SRGB_BLOCK,
        VK_FORMAT_BC7_SRGB_BLOCK,
        VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK,
        VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK,
        VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK,
        VK_FORMAT_ASTC_4x4_SRGB_BLOCK,
        VK_FORMAT_ASTC_5x4_SRGB_BLOCK,
        VK_FORMAT_ASTC_5x5_SRGB_BLOCK,
        VK_FORMAT_ASTC_6x5_SRGB_BLOCK,
        VK_FORMAT_ASTC_6x6_SRGB_BLOCK,
        VK_FORMAT_ASTC_8x5_SRGB_BLOCK,
        VK_FORMAT_ASTC_8x6_SRGB_BLOCK,
        VK_FORMAT_ASTC_8x8_SRGB_BLOCK,
        VK_FORMAT_ASTC_10x5_SRGB_BLOCK,
        VK_FORMAT_ASTC_10x6_SRGB_BLOCK,
        VK_FORMAT_ASTC_10x8_SRGB_BLOCK,
        VK_FORMAT_ASTC_10x10_SRGB_BLOCK,
        VK_FORMAT_ASTC_12x10_SRGB_BLOCK,
        VK_FORMAT_ASTC_12x12_SRGB_BLOCK,
        VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG,
        VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG,
        VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG,
        VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG,
    };

    return contains(srgbFormats, format);
}

void vkutils::CreateSignalledFence(VulkanContext &ctx, VkFence &fence)
{
    VkFenceCreateInfo fence_info = {};
    fence_info.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags             = VK_FENCE_CREATE_SIGNALED_BIT;

    auto ret = vkCreateFence(ctx.Device, &fence_info, nullptr, &fence);

    vassert(ret == VK_SUCCESS, "Failed to create a fence!");
}

void vkutils::CreateSemaphore(VulkanContext &ctx, VkSemaphore &semaphore)
{
    VkSemaphoreCreateInfo semaphore_info = {};
    semaphore_info.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    auto ret = vkCreateSemaphore(ctx.Device, &semaphore_info, nullptr, &semaphore);

    vassert(ret == VK_SUCCESS, "Failed to create a semaphore!");
}


VkCommandPool vkutils::CreateCommandPool(VulkanContext &ctx, QueueType qtype)
{
    VkCommandPool pool{};

    uint32_t queueFamilyId = 0;

    switch (qtype)
    {
    case QueueType::Graphics: {
        queueFamilyId = ctx.Queues.Graphics.Index;
        break;
    }
    case QueueType::Present: {
        queueFamilyId = ctx.Queues.Present.Index;
        break;
    }
    }

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex        = queueFamilyId;
    // To allow resetting individual buffers:
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    auto ret = vkCreateCommandPool(ctx.Device, &poolInfo, nullptr, &pool);

    vassert(ret == VK_SUCCESS, "Failed to create a command pool!");

    return pool;
}

VkCommandBuffer vkutils::AllocateCommandBuffer(VulkanContext &ctx, VkCommandPool pool)
{
    VkCommandBuffer buffer{};

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool        = pool;
    allocInfo.commandBufferCount = 1;

    auto ret = vkAllocateCommandBuffers(ctx.Device, &allocInfo, &buffer);

    vassert(ret == VK_SUCCESS, "Failed to allocate command buffers!");

    return buffer;
}

void vkutils::AllocateCommandBuffers(VulkanContext             &ctx,
                                    std::span<VkCommandBuffer> buffers,
                                    VkCommandPool              pool)
{
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool        = pool;
    allocInfo.commandBufferCount = static_cast<uint32_t>(buffers.size());

    auto ret = vkAllocateCommandBuffers(ctx.Device, &allocInfo, buffers.data());

    vassert(ret == VK_SUCCESS, "Failed to allocate command buffers!");
}

void vkutils::BeginRecording(VkCommandBuffer buffer, VkCommandBufferUsageFlags flags)
{
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags                    = flags;

    auto ret = vkBeginCommandBuffer(buffer, &beginInfo);

    vassert(ret == VK_SUCCESS, "Failed to begin recording command buffer!");
}

void vkutils::EndRecording(VkCommandBuffer buffer)
{
    auto ret = vkEndCommandBuffer(buffer);

    vassert(ret == VK_SUCCESS, "Failed to record command buffer!");
}

void vkutils::BlitImageZeroMip(VkCommandBuffer cmd, const Image &src, const Image &dst)
{
    VkImageBlit2 blitRegion = {};
    blitRegion.sType        = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;

    blitRegion.srcOffsets[1].x = static_cast<int32_t>(src.Info.extent.width);
    blitRegion.srcOffsets[1].y = static_cast<int32_t>(src.Info.extent.height);
    blitRegion.srcOffsets[1].z = static_cast<int32_t>(src.Info.extent.depth);

    blitRegion.dstOffsets[1].x = static_cast<int32_t>(dst.Info.extent.width);
    blitRegion.dstOffsets[1].y = static_cast<int32_t>(dst.Info.extent.height);
    blitRegion.dstOffsets[1].z = static_cast<int32_t>(dst.Info.extent.depth);

    blitRegion.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    blitRegion.srcSubresource.baseArrayLayer = 0;
    blitRegion.srcSubresource.layerCount     = src.Info.arrayLayers;
    blitRegion.srcSubresource.mipLevel       = 0;

    blitRegion.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    blitRegion.dstSubresource.baseArrayLayer = 0;
    blitRegion.dstSubresource.layerCount     = dst.Info.arrayLayers;
    blitRegion.dstSubresource.mipLevel       = 0;

    VkBlitImageInfo2 blitInfo = {};
    blitInfo.sType            = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;

    blitInfo.dstImage       = dst.Handle;
    blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    blitInfo.srcImage       = src.Handle;
    blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    blitInfo.filter         = VK_FILTER_LINEAR;
    blitInfo.regionCount    = 1;
    blitInfo.pRegions       = &blitRegion;

    vkCmdBlitImage2(cmd, &blitInfo);
}

void vkutils::BlitImageZeroMip(VkCommandBuffer cmd, const Image &src, BlitImageInfo dst)
{
    VkImageBlit2 blitRegion = {};
    blitRegion.sType        = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;

    blitRegion.srcOffsets[1].x = static_cast<int32_t>(src.Info.extent.width);
    blitRegion.srcOffsets[1].y = static_cast<int32_t>(src.Info.extent.height);
    blitRegion.srcOffsets[1].z = static_cast<int32_t>(src.Info.extent.depth);

    blitRegion.dstOffsets[1].x = static_cast<int32_t>(dst.Extent.width);
    blitRegion.dstOffsets[1].y = static_cast<int32_t>(dst.Extent.height);
    blitRegion.dstOffsets[1].z = static_cast<int32_t>(dst.Extent.depth);

    blitRegion.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    blitRegion.srcSubresource.baseArrayLayer = 0;
    blitRegion.srcSubresource.layerCount     = src.Info.arrayLayers;
    blitRegion.srcSubresource.mipLevel       = 0;

    blitRegion.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    blitRegion.dstSubresource.baseArrayLayer = 0;
    blitRegion.dstSubresource.layerCount     = src.Info.arrayLayers;
    blitRegion.dstSubresource.mipLevel       = 0;

    VkBlitImageInfo2 blitInfo = {};
    blitInfo.sType            = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;

    blitInfo.dstImage       = dst.ImgHandle;
    blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    blitInfo.srcImage       = src.Handle;
    blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    blitInfo.filter         = VK_FILTER_LINEAR;
    blitInfo.regionCount    = 1;
    blitInfo.pRegions       = &blitRegion;

    vkCmdBlitImage2(cmd, &blitInfo);
}