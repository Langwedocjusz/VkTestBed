#pragma once

#include "VulkanContext.h"

#include "volk.h"

#include <optional>
#include <span>

namespace vkinit
{
void CreateSignalledFence(VulkanContext &ctx, VkFence &fence);
void CreateSemaphore(VulkanContext &ctx, VkSemaphore &semaphore);

VkCommandPool   CreateCommandPool(VulkanContext &ctx, vkb::QueueType qtype);
VkCommandBuffer CreateCommandBuffer(VulkanContext &ctx, VkCommandPool pool);

void AllocateCommandBuffers(VulkanContext &ctx, std::span<VkCommandBuffer> buffers,
                            VkCommandPool pool);

VkRenderingAttachmentInfo CreateAttachmentInfo(
    VkImageView view, VkImageLayout layout,
    std::optional<VkClearValue> clear = std::nullopt);

VkRenderingAttachmentInfo CreateAttachmentInfoMSAA(
    VkImageView viewMsaa, VkImageView viewResolve, VkImageLayout layout,
    std::optional<VkClearValue> clear = std::nullopt);

VkRenderingInfo CreateRenderingInfo(VkExtent2D                 extent,
                                    VkRenderingAttachmentInfo &colorAttachment);

VkRenderingInfo CreateRenderingInfo(VkExtent2D                 extent,
                                    VkRenderingAttachmentInfo &colorAttachment,
                                    VkRenderingAttachmentInfo &depthAttachment,
                                    bool                       hasStencil);

VkRenderingInfo CreateRenderingInfo(VkExtent2D                 extent,
                                    VkRenderingAttachmentInfo &depthAttachment,
                                    bool                       hasStencil);
}; // namespace vkinit