#include "VkInit.h"
#include "Pch.h"

#include "Vassert.h"

#include "volk.h"
#include <vulkan/vulkan_core.h>

void vkinit::CreateSignalledFence(VulkanContext &ctx, VkFence &fence)
{
    VkFenceCreateInfo fence_info = {};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    auto ret = vkCreateFence(ctx.Device, &fence_info, nullptr, &fence);

    vassert(ret == VK_SUCCESS, "Failed to create a fence!");
}

void vkinit::CreateSemaphore(VulkanContext &ctx, VkSemaphore &semaphore)
{
    VkSemaphoreCreateInfo semaphore_info = {};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    auto ret = vkCreateSemaphore(ctx.Device, &semaphore_info, nullptr, &semaphore);

    vassert(ret == VK_SUCCESS, "Failed to create a semaphore!");
}

VkCommandPool vkinit::CreateCommandPool(VulkanContext &ctx, vkb::QueueType qtype)
{
    VkCommandPool pool;

    auto queueFamilyId = ctx.Device.get_queue_index(qtype).value();

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyId;
    // To allow resetting individual buffers:
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    auto ret = vkCreateCommandPool(ctx.Device, &poolInfo, nullptr, &pool);

    vassert(ret == VK_SUCCESS, "Failed to create a command pool!");

    return pool;
}

VkCommandBuffer vkinit::CreateCommandBuffer(VulkanContext &ctx, VkCommandPool pool)
{
    VkCommandBuffer buffer;

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = pool;
    allocInfo.commandBufferCount = 1;

    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    auto ret = vkAllocateCommandBuffers(ctx.Device, &allocInfo, &buffer);

    vassert(ret == VK_SUCCESS, "Failed to allocate command buffers!");

    return buffer;
}

void vkinit::AllocateCommandBuffers(VulkanContext &ctx,
                                    std::span<VkCommandBuffer> buffers,
                                    VkCommandPool pool)
{
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = pool;
    allocInfo.commandBufferCount = static_cast<uint32_t>(buffers.size());

    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    auto ret = vkAllocateCommandBuffers(ctx.Device, &allocInfo, buffers.data());

    vassert(ret == VK_SUCCESS, "Failed to allocate command buffers!");
}

VkRenderingAttachmentInfo vkinit::CreateAttachmentInfo(VkImageView view,
                                                       VkImageLayout layout,
                                                       std::optional<VkClearValue> clear)
{
    VkRenderingAttachmentInfo attachment{};
    attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    attachment.imageView = view;
    attachment.imageLayout = layout;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    // Unused:
    // attachment.resolveImageLayout
    // attachment.resolveImageView
    // attachment.resolveMode

    if (clear.has_value())
    {
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment.clearValue = clear.value();
    }

    return attachment;
}

VkRenderingAttachmentInfo vkinit::CreateAttachmentInfoMSAA(
    VkImageView viewMsaa, VkImageView viewResolve, VkImageLayout layout,
    std::optional<VkClearValue> clear)
{
    VkRenderingAttachmentInfo attachment{};
    attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    attachment.imageView = viewMsaa;
    attachment.imageLayout = layout;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    attachment.resolveImageLayout = layout;
    attachment.resolveImageView = viewResolve;
    // To-do: maybe expose this:
    if (layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        attachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
    else
        attachment.resolveMode = VK_RESOLVE_MODE_MIN_BIT;

    if (clear.has_value())
    {
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment.clearValue = clear.value();
    }

    return attachment;
}

VkRenderingInfo vkinit::CreateRenderingInfo(VkExtent2D extent,
                                            VkRenderingAttachmentInfo &colorAttachment)
{
    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.extent = extent;
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;

    return renderingInfo;
}

VkRenderingInfo vkinit::CreateRenderingInfo(VkExtent2D extent,
                                            VkRenderingAttachmentInfo &colorAttachment,
                                            VkRenderingAttachmentInfo &depthAttachment,
                                            bool hasStencil)
{
    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.extent = extent;
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = &depthAttachment;

    if (hasStencil)
        renderingInfo.pStencilAttachment = &depthAttachment;

    return renderingInfo;
}

VkRenderingInfo vkinit::CreateRenderingInfo(VkExtent2D extent,
                                            VkRenderingAttachmentInfo &depthAttachment,
                                            bool hasStencil)
{
    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.extent = extent;
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.layerCount = 1;
    renderingInfo.pDepthAttachment = &depthAttachment;

    if (hasStencil)
        renderingInfo.pStencilAttachment = &depthAttachment;

    return renderingInfo;
}