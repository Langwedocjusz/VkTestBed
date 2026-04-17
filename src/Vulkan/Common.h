#pragma once

#include <optional>
#include <span>

#include "volk.h"

namespace common
{
void ViewportScissor(VkCommandBuffer buffer, VkExtent2D extent);

// Current render target information.
// Supports at maximum only one color attachment.
// If resolve target is provided the pass is understood to utilize multisampling.
// If both Color and Depth are present and either of {ColorResolve, DepthResolve}
//  is provided the other one must also be given.
// By default it is assumed that depth attachment is also
// a stencil attachment.
// By default clear value is used for all attachments.
// It can be turned off by explicitly passing nullopt.
struct RenderingInfo {
    VkExtent2D                  Extent;
    std::optional<VkImageView>  Color           = std::nullopt;
    std::optional<VkImageView>  Depth           = std::nullopt;
    bool                        DepthHasStencil = true;
    std::optional<VkImageView>  ColorResolve    = std::nullopt;
    std::optional<VkImageView>  DepthResolve    = std::nullopt;
    std::optional<VkClearValue> ClearColor =
        VkClearValue{.color = {{0.0f, 0.0f, 0.0f, 0.0f}}};
    std::optional<VkClearValue> ClearDepth = VkClearValue{
        .depthStencil = {1.0f, 0}
    };
};

void BeginRendering(VkCommandBuffer cmd, RenderingInfo info);

void SubmitQueue(VkQueue queue, VkCommandBuffer cmd, VkFence fence,
                 VkSemaphore waitSemaphore, VkPipelineStageFlags waitStage,
                 VkSemaphore signalSemaphore);

void SubmitQueue(VkQueue queue, std::span<VkCommandBuffer> buffers, VkFence fence,
                 std::span<VkSemaphore>          waitSemaphores,
                 std::span<VkPipelineStageFlags> waitStages,
                 std::span<VkSemaphore>          signalSemaphores);
} // namespace common