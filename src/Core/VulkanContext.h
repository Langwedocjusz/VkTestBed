#pragma once

#include "Frame.h"
#include "SystemWindow.h"

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
    VkInstance       Instance;
    VkPhysicalDevice PhysicalDevice;
    VkDevice         Device;

    struct Queue{
        VkQueue                 Handle     = VK_NULL_HANDLE;
        VkQueueFamilyProperties Properties = {};
        uint32_t                Index      = 0;
    };

    struct Queues {
        Queue Graphics = {};
        Queue Present  = {};
    } Queues;

    VmaAllocator Allocator;
    
    VkSurfaceKHR Surface;

    VkFormat                 SwapchainFormat;
    VkExtent2D               SwapchainExtent;

    std::vector<VkImage>     SwapchainImages;
    std::vector<VkImageView> SwapchainImageViews;

    bool     SwapchainOk = true;
    uint32_t RequestedWidth;
    uint32_t RequestedHeight;

    struct {
      bool  Timestamps         = false;
      bool  PipelineStatistics = false;
      float TimestampPeriod    = 0.0f;
    } OptionalFeatures;

  private:
    struct ExtraData;
    std::unique_ptr<ExtraData> mExtraData;

    VkCommandPool mImmGraphicsCommandPool;
};