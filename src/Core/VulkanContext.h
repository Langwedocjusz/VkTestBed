#pragma once

#include "SystemWindow.h"
#include "VkBootstrap.h"

#include "vk_mem_alloc.h"

class VulkanContext {
  public:
    VulkanContext(uint32_t width, uint32_t height, const std::string &appName,
                  SystemWindow &window);
    ~VulkanContext();

    void CreateSwapchain(bool first_run = false);

  public:
    vkb::Instance Instance;
    vkb::PhysicalDevice PhysicalDevice;
    vkb::Device Device;

    VmaAllocator Allocator;

    VkSurfaceKHR Surface;
    vkb::Swapchain Swapchain;

    std::vector<VkImage> SwapchainImages;
    std::vector<VkImageView> SwapchainImageViews;

    bool SwapchainOk = true;
    uint32_t RequestedWidth;
    uint32_t RequestedHeight;
};