#include "DeletionQueue.h"
#include "Pch.h"

#include "CppUtils.h"

#include <ranges>
#include <vulkan/vulkan.h>

void DeletionQueue::flush()
{
    for (auto obj : mDeletionObjects | std::views::reverse)
    {
        // clang-format off
        std::visit(overloaded{
            [this](VkPipeline arg) {vkDestroyPipeline(mCtx.Device, arg, nullptr);},
            [this](VkPipelineLayout arg) {vkDestroyPipelineLayout(mCtx.Device, arg, nullptr);},
            [this](VkCommandPool arg) {vkDestroyCommandPool(mCtx.Device, arg, nullptr);},
            [this](VkFence arg) {vkDestroyFence(mCtx.Device, arg, nullptr);},
            [this](VkSemaphore arg) {vkDestroySemaphore(mCtx.Device, arg, nullptr);},
            [this](VkImageView arg){vkDestroyImageView(mCtx.Device, arg, nullptr);},
            [this](VkDescriptorPool arg){vkDestroyDescriptorPool(mCtx.Device, arg, nullptr);},
            [this](VkDescriptorSetLayout arg){vkDestroyDescriptorSetLayout(mCtx.Device, arg, nullptr);},
            [this](VkSampler arg){vkDestroySampler(mCtx.Device, arg, nullptr);},
            [this](VkQueryPool arg){vkDestroyQueryPool(mCtx.Device, arg, nullptr);},
            [this](VkAllocatedImage arg){vmaDestroyImage(mCtx.Allocator, arg.Handle, arg.Allocation);},
            [this](VkAllocatedBuffer arg){vmaDestroyBuffer(mCtx.Allocator, arg.Handle, arg.Allocation);},
        }, obj);
        // clang-format on
    }

    mDeletionObjects.clear();
}