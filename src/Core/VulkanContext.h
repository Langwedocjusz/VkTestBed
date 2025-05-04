#pragma once

#include "SystemWindow.h"
#include "VkBootstrap.h"

#include "vk_mem_alloc.h"

#include <vulkan/vulkan.h>

#include <functional>

enum class QueueType
{
    Graphics,
    Present
};

class VulkanContext {
  public:
    VulkanContext(uint32_t width, uint32_t height, const std::string &appName,
                  SystemWindow &window);
    ~VulkanContext();

    void CreateSwapchain(bool firstRun = false);

    VkQueue GetQueue(QueueType type);

    void ImmediateSubmitGraphics(std::function<void(VkCommandBuffer)> &&function);

  public:
    vkb::Instance Instance;
    vkb::PhysicalDevice PhysicalDevice;
    vkb::Device Device;

    struct Queues {
        VkQueue Graphics = VK_NULL_HANDLE;
        VkQueue Present = VK_NULL_HANDLE;
    } Queues;

    struct QueueProperties {
        VkQueueFamilyProperties Graphics;
        VkQueueFamilyProperties Present;
    } QueueProperties;

    VmaAllocator Allocator;

    VkSurfaceKHR Surface;
    vkb::Swapchain Swapchain;

    std::vector<VkImage> SwapchainImages;
    std::vector<VkImageView> SwapchainImageViews;

    bool SwapchainOk = true;
    uint32_t RequestedWidth;
    uint32_t RequestedHeight;

    // Non-core function pointers:
    PFN_vkSetDebugUtilsObjectNameEXT SetDebugUtilsObjectName = VK_NULL_HANDLE;

  private:
    VkCommandPool mImmGraphicsCommandPool;
};