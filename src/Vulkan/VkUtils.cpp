#include "VkUtils.h"

#include <libassert/assert.hpp>
#include <vulkan/vulkan_core.h>

void vkutils::BeginRecording(VkCommandBuffer buffer, VkCommandBufferUsageFlags flags)
{
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = flags;

    auto ret = vkBeginCommandBuffer(buffer, &beginInfo);

    ASSERT(ret == VK_SUCCESS, "Failed to begin recording command buffer!");
}

void vkutils::EndRecording(VkCommandBuffer buffer)
{
    auto ret = vkEndCommandBuffer(buffer);

    ASSERT(ret == VK_SUCCESS, "Failed to record command buffer!");
}

vkutils::ScopedCommand::ScopedCommand(VulkanContext &ctx, QueueType type,
                                      VkCommandPool commandPool)
    : ctx(ctx), mQueue(ctx.GetQueue(type)), mCommandPool(commandPool)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    vkAllocateCommandBuffers(ctx.Device, &allocInfo, &Buffer);

    BeginRecording(Buffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
}

vkutils::ScopedCommand::~ScopedCommand()
{
    EndRecording(Buffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &Buffer;

    vkQueueSubmit(mQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(mQueue);

    vkFreeCommandBuffers(ctx.Device, mCommandPool, 1, &Buffer);
}

void vkutils::BlitImageZeroMip(VkCommandBuffer cmd, const Image &src, const Image &dst)
{
    VkImageBlit2 blitRegion = {};
    blitRegion.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;

    blitRegion.srcOffsets[1].x = static_cast<int32_t>(src.Info.extent.width);
    blitRegion.srcOffsets[1].y = static_cast<int32_t>(src.Info.extent.height);
    blitRegion.srcOffsets[1].z = static_cast<int32_t>(src.Info.extent.depth);

    blitRegion.dstOffsets[1].x = static_cast<int32_t>(dst.Info.extent.width);
    blitRegion.dstOffsets[1].y = static_cast<int32_t>(dst.Info.extent.height);
    blitRegion.dstOffsets[1].z = static_cast<int32_t>(dst.Info.extent.depth);

    blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blitRegion.srcSubresource.baseArrayLayer = 0;
    blitRegion.srcSubresource.layerCount = src.Info.arrayLayers;
    blitRegion.srcSubresource.mipLevel = 0;

    blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blitRegion.dstSubresource.baseArrayLayer = 0;
    blitRegion.dstSubresource.layerCount = dst.Info.arrayLayers;
    blitRegion.dstSubresource.mipLevel = 0;

    VkBlitImageInfo2 blitInfo = {};
    blitInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;

    blitInfo.dstImage = dst.Handle;
    blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    blitInfo.srcImage = src.Handle;
    blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    blitInfo.filter = VK_FILTER_LINEAR;
    blitInfo.regionCount = 1;
    blitInfo.pRegions = &blitRegion;

    vkCmdBlitImage2(cmd, &blitInfo);
}

void vkutils::BlitImageZeroMip(VkCommandBuffer cmd, const Image &src, BlitImageInfo dst)
{
    VkImageBlit2 blitRegion = {};
    blitRegion.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;

    blitRegion.srcOffsets[1].x = static_cast<int32_t>(src.Info.extent.width);
    blitRegion.srcOffsets[1].y = static_cast<int32_t>(src.Info.extent.height);
    blitRegion.srcOffsets[1].z = static_cast<int32_t>(src.Info.extent.depth);

    blitRegion.dstOffsets[1].x = static_cast<int32_t>(dst.Extent.width);
    blitRegion.dstOffsets[1].y = static_cast<int32_t>(dst.Extent.height);
    blitRegion.dstOffsets[1].z = static_cast<int32_t>(dst.Extent.depth);

    blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blitRegion.srcSubresource.baseArrayLayer = 0;
    blitRegion.srcSubresource.layerCount = src.Info.arrayLayers;
    blitRegion.srcSubresource.mipLevel = 0;

    blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blitRegion.dstSubresource.baseArrayLayer = 0;
    blitRegion.dstSubresource.layerCount = src.Info.arrayLayers;
    blitRegion.dstSubresource.mipLevel = 0;

    VkBlitImageInfo2 blitInfo = {};
    blitInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;

    blitInfo.dstImage = dst.ImgHandle;
    blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    blitInfo.srcImage = src.Handle;
    blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    blitInfo.filter = VK_FILTER_LINEAR;
    blitInfo.regionCount = 1;
    blitInfo.pRegions = &blitRegion;

    vkCmdBlitImage2(cmd, &blitInfo);
}