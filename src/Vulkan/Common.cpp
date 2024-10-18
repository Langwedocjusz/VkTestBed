#include "Common.h"

#include <array>

void common::ViewportScissorDefaultBehaviour(VulkanContext &ctx, VkCommandBuffer buffer)
{
    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(ctx.Swapchain.extent.width);
    viewport.height = static_cast<float>(ctx.Swapchain.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(buffer, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = ctx.Swapchain.extent;
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

void common::SubmitGraphicsQueueDefault(VkQueue queue, std::span<VkCommandBuffer> buffers,
                                        VkFence fence, VkSemaphore imageAcquiredSemaphore,
                                        VkSemaphore renderCompleteSemaphore)
{
    std::array<VkSemaphore, 1> waitSemaphores{imageAcquiredSemaphore};
    std::array<VkPipelineStageFlags, 1> waitStages{
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    std::array<VkSemaphore, 1> signalSemaphores{renderCompleteSemaphore};

    common::SubmitQueue(queue, buffers, fence, waitSemaphores, waitStages,
                        signalSemaphores);
}

void common::AcquireNextImage(VulkanContext &ctx, VkSemaphore semaphore,
                              uint32_t &imageIndex)
{
    if (ctx.SwapchainOk)
    {
        VkResult result = vkAcquireNextImageKHR(ctx.Device, ctx.Swapchain, UINT64_MAX,
                                                semaphore, VK_NULL_HANDLE, &imageIndex);

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

void common::PresentFrame(VulkanContext &ctx, VkQueue presentQueue,
                          VkSemaphore renderCompleteSemaphore, uint32_t &frameImageIndex)
{
    std::array<VkSemaphore, 1> signalSemaphores{renderCompleteSemaphore};
    std::array<VkSwapchainKHR, 1> swapChains{ctx.Swapchain};

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size());
    present_info.pWaitSemaphores = signalSemaphores.data();
    present_info.swapchainCount = static_cast<uint32_t>(swapChains.size());
    present_info.pSwapchains = swapChains.data();
    present_info.pImageIndices = &frameImageIndex;

    VkResult result = vkQueuePresentKHR(presentQueue, &present_info);

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

struct ImageMemoryBarrierInfo {
    VkImage Image;
    VkAccessFlags SrcAccessMask;
    VkAccessFlags DstAccessMask;
    VkImageLayout OldImageLayout;
    VkImageLayout NewImageLayout;
    VkPipelineStageFlags SrcStageMask;
    VkPipelineStageFlags DstStageMask;
    VkImageSubresourceRange SubresourceRange;
};

static void InsertImageMemoryBarrier(VkCommandBuffer buffer, ImageMemoryBarrierInfo info)
{
    VkImageMemoryBarrier imageMemoryBarrier{};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    imageMemoryBarrier.srcAccessMask = info.SrcAccessMask;
    imageMemoryBarrier.dstAccessMask = info.DstAccessMask;
    imageMemoryBarrier.oldLayout = info.OldImageLayout;
    imageMemoryBarrier.newLayout = info.NewImageLayout;
    imageMemoryBarrier.image = info.Image;
    imageMemoryBarrier.subresourceRange = info.SubresourceRange;

    vkCmdPipelineBarrier(buffer, info.SrcStageMask, info.DstStageMask, 0, 0, nullptr, 0,
                         nullptr, 1, &imageMemoryBarrier);
}

void common::ImageBarrierColorToRender(VkCommandBuffer buffer, VkImage swapchainImage)
{
    ImageMemoryBarrierInfo info{
        swapchainImage,
        0,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

    InsertImageMemoryBarrier(buffer, info);
}

void common::ImageBarrierColorToPresent(VkCommandBuffer buffer, VkImage swapchainImage)
{
    ImageMemoryBarrierInfo info{
        swapchainImage,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        0,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

    InsertImageMemoryBarrier(buffer, info);
}