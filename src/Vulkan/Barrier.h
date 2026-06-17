#pragma once

#include "Image.h"

#include "volk.h"

#include <optional>

namespace barrier
{
// Barriers for synchronizing stages of swapchain image lifetime:
// ACQUISITION -> BLIT DST -> RENDERING (UI) -> PRESENTATION.
// Since those images are acquired and not created by us
// the api here uses VkImage instead of our Image wrapper.

void SwapchainToBlitDST(VkCommandBuffer cmd, VkImage image);
void SwapchainToRender(VkCommandBuffer cmd, VkImage image);
void SwapchainToPresent(VkCommandBuffer cmd, VkImage image);

// Convention for the naming is as follows:
// Color       => VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
// Depth       => VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
// Sampled     => VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
// TransferSrc => VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL

// Barriers for synchronizing access to intermediate render target
// (color attachment), that gets copied to swapchain
// after rendering:
// ... -> COLOR ATTACHMENT -> BLIT TO SWAPCHAIN -> ...
// TransferSrcToColor discards contents.

void TransferSrcToColor(VkCommandBuffer cmd, VkImage image);
void ColorToTransferSrc(VkCommandBuffer cmd, VkImage image);

// Barriers for synchronizing access to intermediate render target
// (color attachment), that is consumed by compute post processing
// after rendering:
// ... -> COLOR ATTACHMENT -> SAMPLED -> ...
// It is assumed sampling is done in the compute shader.
// SampledToColor discards contents.

void SampledToColor(VkCommandBuffer cmd, VkImage image);
void ColorToSampledComp(VkCommandBuffer cmd, VkImage image);

// Barriers for synchronizing access to depth buffer,
// that is also consumed as a texture:
// ... -> DEPTH ATTACHMENT -> SAMPLED -> ...
// Think shadow map, or depth buffer with no prepass used.
// Sampling can be done either from fragment or compute.
// When using multi-layer depth buffer (shadow cascades),
// number of layers must be provided.

void DepthToRender(VkCommandBuffer cmd, Image &depthImage, uint32_t numLayers = 1);
void DepthToSampledFrag(VkCommandBuffer cmd, Image &depthImage, uint32_t numLayers = 1);
void DepthToSampledComp(VkCommandBuffer cmd, Image &depthImage, uint32_t numLayers = 1);

// Barriers for synchronizing access to depth buffer,
// that is again used as attachment after sampling:
// ... -> DEPTH ATTACHMENT -> SAMPLED -> DEPTH ATTACHMENT -> ...
// Think depth target with prespass, that is first filled,
// then consumed by ssao, then again used to test for final draw.
// The bool parameter msaa specifies if the image is resolve target
// of multi-sampling.

void Depth2ToSampledComp(VkCommandBuffer cmd, Image &depthImage, bool msaa);
void Depth2ToRenderAfterComp(VkCommandBuffer cmd, Image &depthImage, bool msaa);

// Barriers for synchronizing access to auxiliary texture,
// that is generated using compute shaders:
// ... -> GENERAL -> SAMPLED -> ...
// Frag means it is assumed that the texture is read by fragment shaders.
// TextureFragToGeneral and TextureCompToGeneral discard contents.
// TextureFragToGeneralRetained and TextureCompToGeneralRetained don't.

void TextureFragToGeneral(VkCommandBuffer cmd, Image &image,
                          std::optional<VkImageSubresourceRange> range = std::nullopt);
void TextureFragToGeneralRetained(
    VkCommandBuffer cmd, Image &image,
    std::optional<VkImageSubresourceRange> range = std::nullopt);
void TextureFragToSample(VkCommandBuffer cmd, Image &image,
                         std::optional<VkImageSubresourceRange> range = std::nullopt);

void TextureCompToGeneral(VkCommandBuffer cmd, Image &image,
                          std::optional<VkImageSubresourceRange> range = std::nullopt);
void TextureCompToGeneralRetained(
    VkCommandBuffer cmd, Image &image,
    std::optional<VkImageSubresourceRange> range = std::nullopt);
void TextureCompToSample(VkCommandBuffer cmd, Image &image,
                         std::optional<VkImageSubresourceRange> range = std::nullopt);

// Barrier that synchronizes acess to auxiliary texture,
// used strictly by compute shaders:
// ... -> GENERAL (writeonly) -> GENERAL (readonly) -> ...

void TextureGeneralToGeneral(VkCommandBuffer cmd, Image &image,
                             std::optional<VkImageSubresourceRange> range = std::nullopt);

// Barriers for synchronizing access to auxiliary texture,
// that is filled with compute, and then transferred (like postfx -> swapchain).
// ... -> GENERAL -> TRANSFER SRC -> ...
// TransferSrcToGeneral discards contents.

void TransferSrcToGeneral(VkCommandBuffer cmd, VkImage image);
void GeneralToTransferSrc(VkCommandBuffer cmd, VkImage image);

// Customizable barrier that does maximal blocking
// (all read & all write). Probably subomptimal,
// but easiest to use when testing things:

struct LayoutTransitionInfo {
    Image                                 &Image;
    VkImageLayout                          OldLayout;
    VkImageLayout                          NewLayout;
    std::optional<VkImageSubresourceRange> SubresourceRange = std::nullopt;
};

void ImageLayoutCoarse(VkCommandBuffer cmd, LayoutTransitionInfo info);
} // namespace barrier