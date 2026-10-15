#pragma once

#include "Frame.h"
#include "Swapchain.h"
#include "SystemWindow.h"

#include "vk_mem_alloc.h"
#include "volk.h"

#include <functional>
#include <memory>

enum class QueueType
{
    Graphics,
    Present
};

class VulkanContext {
  public:
    VulkanContext(SystemWindow &window);
    ~VulkanContext();

    void ImmediateSubmitGraphics(std::function<void(VkCommandBuffer)> &&function);

  public:
    VkInstance       Instance;
    VkPhysicalDevice PhysicalDevice;
    VkDevice         Device;

    struct Queue {
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
    std::unique_ptr<Swapchain> Swapchain;

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