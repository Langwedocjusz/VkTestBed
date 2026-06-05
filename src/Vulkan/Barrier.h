#pragma once

#include "Image.h"

#include "volk.h"

#include <optional>

namespace barrier
{

// Barriers for synchronizing stages of swapchain image lifetime:
// ACQUISITION -> BLIT DST -> RENDERING -> PRESENTATION.
// Since those images are acquired and not created by us
// the api here uses VkImage instead of our Image.

void SwapchainToBlitDST(VkCommandBuffer cmd, VkImage image);
void SwapchainToRender(VkCommandBuffer cmd, VkImage image);
void SwapchainToPresent(VkCommandBuffer cmd, VkImage image);

// Barriers for synchronizing intermediate render target:
// COLOR ATTACHMENT -> BLIT TO SWAPCHAIN -> COLOR ATTACHMENT -> ...

void TransferSrcToColorAttachment(VkCommandBuffer cmd, VkImage image);
void ColorAttachmentToTransferSrc(VkCommandBuffer cmd, VkImage image);

// Barriers for synchronizing intermediate render target:
// COLOR ATTACHMENT -> SAMPLED -> COLOR ATTACHMENT -> ...

void SampledToColorAttachment(VkCommandBuffer cmd, VkImage image);
void ColorAttachmentToSampled(VkCommandBuffer cmd, VkImage image);

// Barriers for synchronizing depth buffer access:
// DEPTH ATTACHMENT -> SAMPLED -> DEPTH ATTACHMENT -> ...

void DepthToRender(VkCommandBuffer cmd, VkImage depthImage, uint32_t numLayers = 1);
void DepthToSample(VkCommandBuffer cmd, VkImage depthImage, uint32_t numLayers = 1);

struct LayoutTransitionInfo {
    Image                                 &Image;
    VkImageLayout                          OldLayout;
    VkImageLayout                          NewLayout;
    std::optional<VkImageSubresourceRange> SubresourceRange = std::nullopt;
};

void ImageLayoutCoarse(VkCommandBuffer cmd, LayoutTransitionInfo info);
} // namespace barrier