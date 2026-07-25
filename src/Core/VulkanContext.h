#pragma once

#include "Frame.h"
#include "SystemWindow.h"

#include "VkBootstrap.h"
#include "vk_mem_alloc.h"
#include "volk.h"

#include <memory>
#include <functional>

enum class QueueType
{
    Graphics,
    Present
};

struct PresentationInfo{
    VkSemaphore WaitSemaphore   = VK_NULL_HANDLE;
    uint64_t    WaitValue       = 0;
    VkSemaphore SubmitSemaphore = VK_NULL_HANDLE;
    uint64_t    SubmitValue     = 0;
};

class VulkanContext {
  public:
    VulkanContext(uint32_t width, uint32_t height, const std::string &appName,
                  SystemWindow &window);
    ~VulkanContext();

    void CreateSwapchain(bool firstRun = false);
    void ImmediateSubmitGraphics(std::function<void(VkCommandBuffer)> &&function);

    PresentationInfo AcquireSwapchainImage([[maybe_unused]] FrameResources &frameData,
                                           [[maybe_unused]] uint32_t       &imageIndex);
    
    void Present([[maybe_unused]] SwapchainResources &swapResources,
                 [[maybe_unused]] uint32_t           &imageIndex);

  public:
    vkb::Instance       Instance;
    vkb::PhysicalDevice PhysicalDevice;
    vkb::Device         Device;

    struct Queues {
        VkQueue Graphics = VK_NULL_HANDLE;
        VkQueue Present  = VK_NULL_HANDLE;
    } Queues;

    struct QueueProperties {
        VkQueueFamilyProperties Graphics;
        VkQueueFamilyProperties Present;
    } QueueProperties;

    VmaAllocator Allocator;

    VkSurfaceKHR Surface;

    #ifdef VULKAN_ON_DXGI
    struct DxgiData;
    std::unique_ptr<DxgiData> mDxgiData;
    #else    
    vkb::Swapchain Swapchain;
    #endif

    VkFormat                 SwapchainFormat;
    VkExtent2D               SwapchainExtent;

    std::vector<VkImage>     SwapchainImages;
    std::vector<VkImageView> SwapchainImageViews;

    bool     SwapchainOk = true;
    uint32_t RequestedWidth;
    uint32_t RequestedHeight;

  private:
    VkCommandPool mImmGraphicsCommandPool;
};