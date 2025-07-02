#pragma once

#include <array>
#include <vector>

#include <vulkan/vulkan.h>

struct FrameResources {
    VkFence InFlightFence;
    VkSemaphore ImageAcquiredSemaphore;

    VkCommandPool CommandPool;
    VkCommandBuffer CommandBuffer;
};

struct SwapchainResources {
    VkSemaphore RenderCompletedSemaphore;
};

struct FrameStats {
    float CPUTime = 0.0f;
    float GPUTime = 0.0f;
    uint32_t NumTriangles = 0;
    uint32_t NumDraws = 0;
    uint32_t NumBinds = 0;
    uint32_t NumDispatches = 0;
    size_t MemoryUsage = 0;
};

struct FrameInfo {
    static constexpr size_t MaxInFlight = 3;

    std::array<FrameResources, MaxInFlight> FrameData;
    std::vector<SwapchainResources> SwapchainData;

    size_t FrameNumber = 0;
    uint32_t Index = 0;
    uint32_t ImageIndex = 0;

    FrameStats Stats;

    auto &CurrentFrameData()
    {
        return FrameData[Index];
    }
    auto &CurrentPool()
    {
        return FrameData[Index].CommandPool;
    }
    auto &CurrentCmd()
    {
        return FrameData[Index].CommandBuffer;
    }

    auto &CurrentSwapchainData()
    {
        return SwapchainData[ImageIndex];
    }
};