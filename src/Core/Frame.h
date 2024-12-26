#pragma once

#include <array>
#include <vulkan/vulkan.h>

struct FrameData {
    VkFence InFlightFence;
    VkSemaphore ImageAcquiredSemaphore;
    VkSemaphore RenderCompletedSemaphore;

    VkCommandPool CommandPool;
    VkCommandBuffer CommandBuffer;
};

struct FrameInfo {
    static constexpr size_t MaxInFlight = 2;

    std::array<FrameData, MaxInFlight> Data;

    size_t FrameNumber = 0;
    size_t Index = 0;
    uint32_t ImageIndex = 0;

    auto &CurrentData()
    {
        return Data[Index];
    }
    auto &CurrentCmd()
    {
        return Data[Index].CommandBuffer;
    }
    auto &CurrentPool()
    {
        return Data[Index].CommandPool;
    }
};