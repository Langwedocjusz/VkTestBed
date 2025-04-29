#include "Common.h"

#include <array>

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
        assert(waitSemaphores.size() == waitStages.size());

        submitInfo.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
        submitInfo.pWaitSemaphores = waitSemaphores.data();
        submitInfo.pWaitDstStageMask = waitStages.data();
    }

    if (!signalSemaphores.empty())
    {
        submitInfo.signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size());
        submitInfo.pSignalSemaphores = signalSemaphores.data();
    }

    if (vkQueueSubmit(queue, 1, &submitInfo, fence) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to submit commands to queue!");
    }
}

void common::SubmitGraphicsQueue(VulkanContext &ctx,
                                 std::span<VkCommandBuffer> buffers, FrameData &frame)
{
    auto queue = ctx.GetQueue(QueueType::Graphics);

    std::array<VkSemaphore, 1> waitSemaphores{frame.ImageAcquiredSemaphore};
    std::array<VkPipelineStageFlags, 1> waitStages{
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    std::array<VkSemaphore, 1> signalSemaphores{frame.RenderCompletedSemaphore};

    SubmitQueue(queue, buffers, frame.InFlightFence, waitSemaphores, waitStages,
                signalSemaphores);
}

void common::SubmitGraphicsQueue(VulkanContext &ctx, VkCommandBuffer buffer,
                                 FrameData &frame)
{
    auto buffers = std::array<VkCommandBuffer, 1>{buffer};

    SubmitGraphicsQueue(ctx, buffers, frame);
}

void common::AcquireNextImage(VulkanContext &ctx, FrameInfo &frame)
{
    auto semaphore = frame.CurrentData().ImageAcquiredSemaphore;

    if (ctx.SwapchainOk)
    {
        VkResult result =
            vkAcquireNextImageKHR(ctx.Device, ctx.Swapchain, UINT64_MAX, semaphore,
                                  VK_NULL_HANDLE, &frame.ImageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            ctx.SwapchainOk = false;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            throw std::runtime_error("Failed to acquire swapchain image!");
        }
    }
}

void common::PresentFrame(VulkanContext &ctx, FrameInfo &frame)
{
    auto queue = ctx.GetQueue(QueueType::Present);
    auto renderSemaphore = frame.CurrentData().RenderCompletedSemaphore;

    std::array<VkSemaphore, 1> waitSemaphores{renderSemaphore};
    std::array<VkSwapchainKHR, 1> swapChains{ctx.Swapchain};

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
    present_info.pWaitSemaphores = waitSemaphores.data();
    present_info.swapchainCount = static_cast<uint32_t>(swapChains.size());
    present_info.pSwapchains = swapChains.data();
    present_info.pImageIndices = &frame.ImageIndex;

    VkResult result = vkQueuePresentKHR(queue, &present_info);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        ctx.SwapchainOk = false;
        return;
    }
    else if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to present swapchain image!");
    }
}