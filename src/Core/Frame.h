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

struct FrameStats {
    float CPUTime = 0.0f;
    float GPUTime = 0.0f;
    uint32_t NumDraws = 0;
    uint32_t NumTriangles = 0;
    uint32_t NumDispatches = 0;
    size_t MemoryBudget = 0;
    size_t MemoryUsage = 0;
};

struct FrameInfo {
    static constexpr size_t MaxInFlight = 2;

    std::array<FrameData, MaxInFlight> Data;

    size_t FrameNumber = 0;
    uint32_t Index = 0;
    uint32_t ImageIndex = 0;

    FrameStats Stats;

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