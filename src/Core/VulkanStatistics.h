#pragma once

#include "DeletionQueue.h"
#include "Frame.h"
#include "VulkanContext.h"

#include <cstdint>
#include <optional>

struct StatisticsResult {
    std::optional<float> FrameTimeMS;
    std::optional<uint64_t> FragmentInvocations;
};

class VulkanStatisticsCollector {
  public:
    VulkanStatisticsCollector(VulkanContext &ctx);
    ~VulkanStatisticsCollector();

    StatisticsResult QueryResults(uint32_t frameIdx);

    void TimestampTop(VkCommandBuffer cmd, uint32_t frameIdx);
    void TimestampBottom(VkCommandBuffer cmd, uint32_t frameIdx);

    void PipelineStatsStart(VkCommandBuffer cmd, uint32_t frameIdx);
    void PipelineStatsEnd(VkCommandBuffer cmd, uint32_t frameIdx);

  private:
    VulkanContext &mCtx;

    // Timestamps:
    bool mTimestampSupported;
    float mTimestampPeriod;

    struct Timestamp {
        uint64_t Value = 0;
        uint64_t Availability = 0;
    };

    static constexpr size_t TimestampsPerFrame = 2;
    using FrameTimestamps = std::array<Timestamp, TimestampsPerFrame>;

    // Pipeline statistics stuff:
    bool mPipelineStatisticsSupported;

    static constexpr std::array QueriedStatistics{
        VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT};

    struct Statistic {
        uint64_t Value = 0;
        uint64_t Availability = 0;
    };

    using StatisticsData = std::array<Statistic, QueriedStatistics.size()>;

    // Combined per frame resources:
    struct FrameResources {
        VkQueryPool TimestampQueryPool;
        FrameTimestamps Timestamps;
        bool TimestampsFirstRun = true;
        bool WriteTimestamps = false;
        VkQueryPool StatisticsQueryPool;
        StatisticsData Statistics;
    };

    std::array<FrameResources, FrameInfo::MaxInFlight> mResources;

    DeletionQueue mDeletionQueue;
};