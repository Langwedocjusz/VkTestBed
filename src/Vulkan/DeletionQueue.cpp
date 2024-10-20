#include "DeletionQueue.h"
#include "Image.h"

#include <ranges>

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

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
            [this](Image* arg){Image::DestroyImage(mCtx, *arg);},
            [this](VkImageView arg){vkDestroyImageView(mCtx.Device, arg, nullptr);},
            [this](VkDescriptorPool arg){vkDestroyDescriptorPool(mCtx.Device, arg, nullptr);},
        }, obj);
        // clang-format on
    }

    mDeletionObjects.clear();
}