#include "VkUtils.h"
#include "Pch.h"

#include "Vassert.h"

#include <set>

VkImageAspectFlags vkutils::GetDefaultAspect(VkFormat format)
{
    // Check if contains both depth and stencil:
    const std::set<VkFormat> depthStencilFormats{
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM_S8_UINT,
    };
    if (depthStencilFormats.contains(format))
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

    // Otherwise check if depth only:
    const std::set<VkFormat> depthFormats{
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D16_UNORM,
    };
    if (depthFormats.contains(format))
        return VK_IMAGE_ASPECT_DEPTH_BIT;

    // Otherwise check if stencil only:
    const std::set<VkFormat> stencilFormats{
        VK_FORMAT_S8_UINT,
    };
    if (stencilFormats.contains(format))
        return VK_IMAGE_ASPECT_STENCIL_BIT;

    // Otherwise assume color:
    return VK_IMAGE_ASPECT_COLOR_BIT;
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

void vkutils::SubmitQueue(VkQueue queue, VkCommandBuffer cmd, VkFence fence,
                          VkSemaphore waitSemaphore, VkPipelineStageFlags waitStage,
                          VkSemaphore signalSemaphore)
{
    VkSubmitInfo submitInfo = {};
    submitInfo.sType        = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmd;

    if (waitSemaphore != nullptr)
    {
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores    = &waitSemaphore;
        submitInfo.pWaitDstStageMask  = &waitStage;
    }

    if (signalSemaphore != nullptr)
    {
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores    = &signalSemaphore;
    }

    auto submitRes = vkQueueSubmit(queue, 1, &submitInfo, fence);

    vassert(submitRes == VK_SUCCESS, "Failed to submit commands to queue!");
}

// void vkutils::SubmitQueue(VkQueue queue, std::span<VkCommandBuffer> buffers, VkFence
// fence,
//                          std::span<VkSemaphore>          waitSemaphores,
//                          std::span<VkPipelineStageFlags> waitStages,
//                          std::span<VkSemaphore>          signalSemaphores)
//{
//     VkSubmitInfo submitInfo = {};
//     submitInfo.sType        = VK_STRUCTURE_TYPE_SUBMIT_INFO;
//
//     submitInfo.commandBufferCount = static_cast<uint32_t>(buffers.size());
//     submitInfo.pCommandBuffers    = buffers.data();
//
//     if (!waitSemaphores.empty())
//     {
//         vassert(waitSemaphores.size() == waitStages.size());
//
//         submitInfo.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
//         submitInfo.pWaitSemaphores    = waitSemaphores.data();
//         submitInfo.pWaitDstStageMask  = waitStages.data();
//     }
//
//     if (!signalSemaphores.empty())
//     {
//         submitInfo.signalSemaphoreCount =
//         static_cast<uint32_t>(signalSemaphores.size()); submitInfo.pSignalSemaphores =
//         signalSemaphores.data();
//     }
//
//     auto submitRes = vkQueueSubmit(queue, 1, &submitInfo, fence);
//
//     vassert(submitRes == VK_SUCCESS, "Failed to submit commands to queue!");
// }

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