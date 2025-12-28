#include "Common.h"
#include "Pch.h"

#include "Vassert.h"
#include "VkInit.h"

#include <vulkan/vulkan.h>

void common::ViewportScissor(VkCommandBuffer buffer, VkExtent2D extent)
{
    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(buffer, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = extent;
    vkCmdSetScissor(buffer, 0, 1, &scissor);
}

void common::BeginRenderingColor(VkCommandBuffer cmd, VkExtent2D extent,
                                 VkImageView color, bool clear)
{
    std::optional<VkClearValue> colorClear = std::nullopt;

    if (clear)
    {
        colorClear = VkClearValue{};
        colorClear->color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    }

    auto colorAttachment = vkinit::CreateAttachmentInfo(
        color, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, colorClear);

    VkRenderingInfo renderingInfo = vkinit::CreateRenderingInfo(extent, colorAttachment);

    vkCmdBeginRendering(cmd, &renderingInfo);
}

void common::BeginRenderingColorDepth(VkCommandBuffer cmd, VkExtent2D extent,
                                      VkImageView color, VkImageView depth,
                                      bool hasStencil, bool clear)
{
    std::optional<VkClearValue> colorClear = std::nullopt;
    std::optional<VkClearValue> depthClear = std::nullopt;

    if (clear)
    {
        colorClear = VkClearValue{};
        colorClear->color = {{0.0f, 0.0f, 0.0f, 0.0f}};

        depthClear = VkClearValue{};
        depthClear->depthStencil = {1.0f, 0};
    }

    auto colorAttachment = vkinit::CreateAttachmentInfo(
        color, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, colorClear);

    auto depthAttachment = vkinit::CreateAttachmentInfo(
        depth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, depthClear);

    VkRenderingInfo renderingInfo =
        vkinit::CreateRenderingInfo(extent, colorAttachment, depthAttachment, hasStencil);

    vkCmdBeginRendering(cmd, &renderingInfo);
}

void common::BeginRenderingColorDepthMSAA(VkCommandBuffer cmd, VkExtent2D extent,
                                          VkImageView colorMsaa, VkImageView colorResolve,
                                          VkImageView depthMsaa, VkImageView depthResolve,
                                          bool hasStencil, bool clear)
{
    std::optional<VkClearValue> colorClear = std::nullopt;
    std::optional<VkClearValue> depthClear = std::nullopt;

    if (clear)
    {
        colorClear = VkClearValue{};
        colorClear->color = {{0.0f, 0.0f, 0.0f, 0.0f}};

        depthClear = VkClearValue{};
        depthClear->depthStencil = {1.0f, 0};
    }

    auto colorAttachment = vkinit::CreateAttachmentInfoMSAA(
        colorMsaa, colorResolve, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, colorClear);

    auto depthAttachment = vkinit::CreateAttachmentInfoMSAA(
        depthMsaa, depthResolve, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        depthClear);

    VkRenderingInfo renderingInfo =
        vkinit::CreateRenderingInfo(extent, colorAttachment, depthAttachment, hasStencil);

    vkCmdBeginRendering(cmd, &renderingInfo);
}

void common::BeginRenderingDepth(VkCommandBuffer cmd, VkExtent2D extent,
                                 VkImageView depth, bool hasStencil, bool clear)
{
    std::optional<VkClearValue> depthClear = std::nullopt;

    if (clear)
    {
        depthClear = VkClearValue{};
        depthClear->depthStencil = {1.0f, 0};
    }

    auto depthAttachment = vkinit::CreateAttachmentInfo(
        depth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, depthClear);

    VkRenderingInfo renderingInfo =
        vkinit::CreateRenderingInfo(extent, depthAttachment, hasStencil);

    vkCmdBeginRendering(cmd, &renderingInfo);
}

void common::BeginRenderingDepthMSAA(VkCommandBuffer cmd, VkExtent2D extent,
                                     VkImageView depthMsaa, VkImageView depthResolve,
                                     bool hasStencil, bool clear)
{
    std::optional<VkClearValue> depthClear = std::nullopt;

    if (clear)
    {
        depthClear = VkClearValue{};
        depthClear->depthStencil = {1.0f, 0};
    }

    auto depthAttachment = vkinit::CreateAttachmentInfoMSAA(
        depthMsaa, depthResolve, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        depthClear);

    VkRenderingInfo renderingInfo =
        vkinit::CreateRenderingInfo(extent, depthAttachment, hasStencil);

    vkCmdBeginRendering(cmd, &renderingInfo);
}

void common::SubmitQueue(VkQueue queue, VkCommandBuffer cmd, VkFence fence,
                         VkSemaphore waitSemaphore, VkPipelineStageFlags waitStage,
                         VkSemaphore signalSemaphore)
{
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    if (waitSemaphore != nullptr)
    {
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &waitSemaphore;
        submitInfo.pWaitDstStageMask = &waitStage;
    }

    if (signalSemaphore != nullptr)
    {
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &signalSemaphore;
    }

    auto submitRes = vkQueueSubmit(queue, 1, &submitInfo, fence);

    vassert(submitRes == VK_SUCCESS, "Failed to submit commands to queue!");
}

void common::SubmitQueue(VkQueue queue, std::span<VkCommandBuffer> buffers, VkFence fence,
                         std::span<VkSemaphore> waitSemaphores,
                         std::span<VkPipelineStageFlags> waitStages,
                         std::span<VkSemaphore> signalSemaphores)
{
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    submitInfo.commandBufferCount = static_cast<uint32_t>(buffers.size());
    submitInfo.pCommandBuffers = buffers.data();

    if (!waitSemaphores.empty())
    {
        vassert(waitSemaphores.size() == waitStages.size());

        submitInfo.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
        submitInfo.pWaitSemaphores = waitSemaphores.data();
        submitInfo.pWaitDstStageMask = waitStages.data();
    }

    if (!signalSemaphores.empty())
    {
        submitInfo.signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size());
        submitInfo.pSignalSemaphores = signalSemaphores.data();
    }

    auto submitRes = vkQueueSubmit(queue, 1, &submitInfo, fence);

    vassert(submitRes == VK_SUCCESS, "Failed to submit commands to queue!");
}