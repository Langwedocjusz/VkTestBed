#include "Common.h"
#include "Pch.h"

#include "Vassert.h"

#include "volk.h"

void common::ViewportScissor(VkCommandBuffer buffer, VkExtent2D extent)
{
    VkViewport viewport = {};
    viewport.x          = 0.0f;
    viewport.y          = 0.0f;
    viewport.width      = static_cast<float>(extent.width);
    viewport.height     = static_cast<float>(extent.height);
    viewport.minDepth   = 0.0f;
    viewport.maxDepth   = 1.0f;
    vkCmdSetViewport(buffer, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset   = {0, 0};
    scissor.extent   = extent;
    vkCmdSetScissor(buffer, 0, 1, &scissor);
}

static VkRenderingAttachmentInfo CreateAttachmentInfo(
    VkImageView baseView, std::optional<VkImageView> resolveView, VkImageLayout layout,
    std::optional<VkClearValue> clear)
{
    VkRenderingAttachmentInfo attachment{};
    attachment.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    attachment.imageView   = baseView;
    attachment.imageLayout = layout;

    // If clear is not set we preserve previous contents of
    // the render target. This allows easy chaining of multiple
    // passes, but in some cases it could be more performant
    // to use LOAD_OP_DONT_CARE.
    attachment.loadOp  = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    if (clear.has_value())
    {
        attachment.loadOp     = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment.clearValue = clear.value();
    }

    if (resolveView.has_value())
    {
        attachment.resolveImageLayout = layout;
        attachment.resolveImageView   = *resolveView;

        // TODO: maybe expose this:
        // Selecting default methods of MSAA resolve based on layout:
        if (layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            attachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
        else
            attachment.resolveMode = VK_RESOLVE_MODE_MIN_BIT;
    }

    return attachment;
}

void common::BeginRendering(VkCommandBuffer cmd, RenderingInfo info)
{
    bool hasColor        = info.Color.has_value();
    bool hasDepth        = info.Depth.has_value();
    bool hasColorResolve = info.ColorResolve.has_value();
    bool hasDepthResolve = info.DepthResolve.has_value();

    if (hasColor && hasDepth)
        vassert(hasColorResolve == hasDepthResolve,
                "If any resolve target is present, the other one must be as well.");

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType             = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.extent = info.Extent;
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.layerCount        = 1;

    VkRenderingAttachmentInfo colorAttachment{};

    if (info.Color.has_value())
    {
        colorAttachment = CreateAttachmentInfo(*info.Color, info.ColorResolve,
                                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                               info.ClearColor);

        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments    = &colorAttachment;
    }

    VkRenderingAttachmentInfo depthAttachment{};

    if (info.Depth.has_value())
    {
        depthAttachment = CreateAttachmentInfo(
            *info.Depth, info.DepthResolve,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, info.ClearDepth);

        renderingInfo.pDepthAttachment = &depthAttachment;

        if (info.DepthHasStencil)
            renderingInfo.pStencilAttachment = &depthAttachment;
    }

    vkCmdBeginRendering(cmd, &renderingInfo);
}