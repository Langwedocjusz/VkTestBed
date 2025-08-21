#include "Common.h"
#include "Pch.h"

#include "Vassert.h"

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

void common::SubmitQueue(VkQueue queue, VkCommandBuffer *cmdBuffer, VkFence fence,
                         VkSemaphore *waitSemaphore, VkPipelineStageFlags *waitStage,
                         VkSemaphore *signalSemaphore)
{
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = cmdBuffer;

    if (waitSemaphore != nullptr)
    {
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphore;
        submitInfo.pWaitDstStageMask = waitStage;
    }

    if (signalSemaphore != nullptr)
    {
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphore;
    }

    auto submitRes = vkQueueSubmit(queue, 1, &submitInfo, fence);

    vassert(submitRes == VK_SUCCESS, "Failed to submit commands to queue!");
}