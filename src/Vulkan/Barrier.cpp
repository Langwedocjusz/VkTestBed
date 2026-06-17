#include "Barrier.h"
#include "Pch.h"

#include "VkUtils.h"

#include "volk.h"

// Small utility to make writing the rest of those less painful:
static void ImageBarrier(VkCommandBuffer cmd, VkImageMemoryBarrier2 barrier)
{
    VkDependencyInfo depInfo{};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.pNext = nullptr;

    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers    = &barrier;

    vkCmdPipelineBarrier2(cmd, &depInfo);
}

void barrier::SwapchainToBlitDST(VkCommandBuffer cmd, VkImage image)
{
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

    // This is first swapchain-related image barrier issued,
    // but it needs to wait for vkAcqureNextImageKHR.
    // (Leaving src stage mask empty results in a WRITE_AFTER_READ hazard)
    // This is synchronized via the semaphore wait stage set in
    // vkQueueSubmit. The logical choice there (the latest possible)
    // is color attachment output, so this is mirrored here:

    barrier.srcStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_NONE_KHR;

    barrier.dstStageMask  = VK_PIPELINE_STAGE_2_BLIT_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;

    // At this point we don't care about swapchain image contents
    // (we will redraw everything) so src layout can be set to undefined:
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    barrier.image = image;
    barrier.subresourceRange =
        VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    ImageBarrier(cmd, barrier);
}

void barrier::SwapchainToRender(VkCommandBuffer cmd, VkImage image)
{
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

    barrier.srcStageMask  = VK_PIPELINE_STAGE_2_BLIT_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;

    barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.dstAccessMask =
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    barrier.image = image;
    barrier.subresourceRange =
        VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    ImageBarrier(cmd, barrier);
}

void barrier::SwapchainToPresent(VkCommandBuffer cmd, VkImage image)
{
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

    barrier.srcStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

    barrier.dstStageMask  = VK_PIPELINE_STAGE_2_NONE;
    barrier.dstAccessMask = VK_ACCESS_2_NONE;

    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    barrier.image = image;
    barrier.subresourceRange =
        VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    ImageBarrier(cmd, barrier);
}

void barrier::TransferSrcToColor(VkCommandBuffer cmd, VkImage image)
{
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

    barrier.srcStageMask  = VK_PIPELINE_STAGE_2_BLIT_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;

    barrier.dstStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

    // At this point we don't care about color attachment contents
    // (we will redraw everything) so src layout can be set to undefined:
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    barrier.image = image;
    barrier.subresourceRange =
        VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    ImageBarrier(cmd, barrier);
}

void barrier::ColorToTransferSrc(VkCommandBuffer cmd, VkImage image)
{
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

    barrier.srcStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

    barrier.dstStageMask  = VK_PIPELINE_STAGE_2_BLIT_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;

    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    barrier.image = image;
    barrier.subresourceRange =
        VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    ImageBarrier(cmd, barrier);
}

void barrier::SampledToColor(VkCommandBuffer cmd, VkImage image)
{
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

    barrier.srcStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

    barrier.dstStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

    // At this point we don't care about color attachment contents
    // (we will redraw everything) so src layout can be set to undefined:
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    barrier.image = image;
    barrier.subresourceRange =
        VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    ImageBarrier(cmd, barrier);
}

void barrier::ColorToSampledComp(VkCommandBuffer cmd, VkImage image)
{
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

    barrier.srcStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

    barrier.dstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    barrier.image = image;
    barrier.subresourceRange =
        VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    ImageBarrier(cmd, barrier);
}

void barrier::DepthToRender(VkCommandBuffer cmd, Image &depthImage, uint32_t numLayers)
{
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

    barrier.srcStageMask  = VK_PIPELINE_STAGE_2_NONE;
    barrier.srcAccessMask = VK_ACCESS_2_NONE;

    barrier.dstStageMask  = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    // At this point we don't care about depth buffer contents
    // (we will redraw everything) so src layout can be set to undefined:
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // TODO: Does it matter if depth-only (no stencil)
    // buffers get a more fine grained layout
    // (VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ?
    barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    barrier.image            = depthImage.Handle;
    barrier.subresourceRange = VkImageSubresourceRange{
        vkutils::GetDefaultAspect(depthImage.Info.format), 0, 1, 0, numLayers};

    ImageBarrier(cmd, barrier);
}

void barrier::DepthToSampledFrag(VkCommandBuffer cmd, Image &depthImage,
                                 uint32_t numLayers)
{
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

    barrier.srcStageMask  = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    barrier.dstStageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

    barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    barrier.image            = depthImage.Handle;
    barrier.subresourceRange = VkImageSubresourceRange{
        vkutils::GetDefaultAspect(depthImage.Info.format), 0, 1, 0, numLayers};

    ImageBarrier(cmd, barrier);
}

void barrier::DepthToSampledComp(VkCommandBuffer cmd, Image &depthImage,
                                 uint32_t numLayers)
{
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

    barrier.srcStageMask  = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    barrier.dstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

    barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    barrier.image            = depthImage.Handle;
    barrier.subresourceRange = VkImageSubresourceRange{
        vkutils::GetDefaultAspect(depthImage.Info.format), 0, 1, 0, numLayers};

    ImageBarrier(cmd, barrier);
}

void barrier::Depth2ToSampledComp(VkCommandBuffer cmd, Image &depthImage, bool msaa)
{
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

    // NOTE: Resolving msaa image (even depth/stencil) classifies as color attachment
    // write. See Vulkan Specification 8.7. Render Pass Multisample Resolve Operations.
    if (msaa)
    {
        barrier.srcStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    }
    else
    {
        barrier.srcStageMask  = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        barrier.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }

    barrier.dstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

    barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    barrier.image            = depthImage.Handle;
    barrier.subresourceRange = VkImageSubresourceRange{
        vkutils::GetDefaultAspect(depthImage.Info.format), 0, 1, 0, 1};

    ImageBarrier(cmd, barrier);
}

void barrier::Depth2ToRenderAfterComp(VkCommandBuffer cmd, Image &depthImage, bool msaa)
{
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

    barrier.srcStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

    // NOTE: Resolving msaa image (even depth/stencil) classifies as color attachment
    // write. See Vulkan Specification 8.7. Render Pass Multisample Resolve Operations.
    if (msaa)
    {
        barrier.dstStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    }
    else
    {
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
        // We must use both read and write for future depth comparison:
        barrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }

    barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    barrier.image            = depthImage.Handle;
    barrier.subresourceRange = VkImageSubresourceRange{
        vkutils::GetDefaultAspect(depthImage.Info.format), 0, 1, 0, 1};

    ImageBarrier(cmd, barrier);
}

void barrier::TextureFragToGeneral(VkCommandBuffer cmd, Image &image,
                                   std::optional<VkImageSubresourceRange> range)
{
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

    barrier.srcStageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

    barrier.dstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;

    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;

    barrier.image = image.Handle;

    if (range.has_value())
        barrier.subresourceRange = *range;
    else
        barrier.subresourceRange = Image::GetDefaultRange(image);

    ImageBarrier(cmd, barrier);
}

void barrier::TextureFragToGeneralRetained(VkCommandBuffer cmd, Image &image,
                                           std::optional<VkImageSubresourceRange> range)
{
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

    barrier.srcStageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

    barrier.dstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;

    barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;

    barrier.image = image.Handle;

    if (range.has_value())
        barrier.subresourceRange = *range;
    else
        barrier.subresourceRange = Image::GetDefaultRange(image);

    ImageBarrier(cmd, barrier);
}

void barrier::TextureFragToSample(VkCommandBuffer cmd, Image &image,
                                  std::optional<VkImageSubresourceRange> range)
{
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

    barrier.srcStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;

    barrier.dstStageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    barrier.image = image.Handle;

    if (range.has_value())
        barrier.subresourceRange = *range;
    else
        barrier.subresourceRange = Image::GetDefaultRange(image);

    ImageBarrier(cmd, barrier);
}

void barrier::TextureCompToGeneral(VkCommandBuffer cmd, Image &image,
                                   std::optional<VkImageSubresourceRange> range)
{
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

    barrier.srcStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

    barrier.dstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;

    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;

    barrier.image = image.Handle;

    if (range.has_value())
        barrier.subresourceRange = *range;
    else
        barrier.subresourceRange = Image::GetDefaultRange(image);

    ImageBarrier(cmd, barrier);
}

void barrier::TextureCompToGeneralRetained(VkCommandBuffer cmd, Image &image,
                                           std::optional<VkImageSubresourceRange> range)
{
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

    barrier.srcStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

    barrier.dstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;

    barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;

    barrier.image = image.Handle;

    if (range.has_value())
        barrier.subresourceRange = *range;
    else
        barrier.subresourceRange = Image::GetDefaultRange(image);

    ImageBarrier(cmd, barrier);
}

void barrier::TextureCompToSample(VkCommandBuffer cmd, Image &image,
                                  std::optional<VkImageSubresourceRange> range)
{
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

    barrier.srcStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;

    barrier.dstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    barrier.image = image.Handle;

    if (range.has_value())
        barrier.subresourceRange = *range;
    else
        barrier.subresourceRange = Image::GetDefaultRange(image);

    ImageBarrier(cmd, barrier);
}

void barrier::TextureGeneralToGeneral(VkCommandBuffer cmd, Image &image,
                                      std::optional<VkImageSubresourceRange> range)
{
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

    barrier.srcStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;

    barrier.dstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;

    barrier.image = image.Handle;

    if (range.has_value())
        barrier.subresourceRange = *range;
    else
        barrier.subresourceRange = Image::GetDefaultRange(image);

    ImageBarrier(cmd, barrier);
}

void barrier::TransferSrcToGeneral(VkCommandBuffer cmd, VkImage image)
{
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

    barrier.srcStageMask  = VK_PIPELINE_STAGE_2_BLIT_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;

    barrier.dstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;

    // At this point we don't care about color attachment contents
    // (we will redraw everything) so src layout can be set to undefined:
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;

    barrier.image = image;
    barrier.subresourceRange =
        VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    ImageBarrier(cmd, barrier);
}

void barrier::GeneralToTransferSrc(VkCommandBuffer cmd, VkImage image)
{
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

    barrier.srcStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;

    barrier.dstStageMask  = VK_PIPELINE_STAGE_2_BLIT_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;

    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    barrier.image = image;
    barrier.subresourceRange =
        VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    ImageBarrier(cmd, barrier);
}

void barrier::ImageLayoutCoarse(VkCommandBuffer cmd, LayoutTransitionInfo info)
{
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

    barrier.oldLayout = info.OldLayout;
    barrier.newLayout = info.NewLayout;
    barrier.image     = info.Image.Handle;

    if (info.SubresourceRange)
    {
        barrier.subresourceRange = *info.SubresourceRange;
    }
    else
    {
        // By default assume all image layers/levels are to be transitioned:
        barrier.subresourceRange = Image::GetDefaultRange(info.Image);
    }

    // Suboptimal barrier usage - block everything:
    barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
    barrier.srcStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    barrier.dstStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

    ImageBarrier(cmd, barrier);
}