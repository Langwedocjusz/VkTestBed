#include "VulkanStatistics.h"
#include "Pch.h"

#include "Vassert.h"

#include "volk.h"

#include <iostream>

VulkanStatisticsCollector::VulkanStatisticsCollector(VulkanContext &ctx)
    : mCtx(ctx), mDeletionQueue(ctx)
{
    // Check for timestamp support:
    auto &limits = mCtx.PhysicalDevice.properties.limits;

    mTimestampPeriod = limits.timestampPeriod;
    mTimestampSupported = (mTimestampPeriod != 0.0f);

    if (!limits.timestampComputeAndGraphics)
    {
        if (mCtx.QueueProperties.Graphics.timestampValidBits == 0)
        {
            mTimestampSupported = false;
        }
    }

    if (!mTimestampSupported)
        std::cout << "Timestamp queries not supported!\n";

    // Check for pipeline statistics support:
    mPipelineStatisticsSupported = mCtx.PhysicalDevice.features.pipelineStatisticsQuery;

    if (!mPipelineStatisticsSupported)
        std::cout << "Pipeline statistics queries not supported!\n";

    // Setup timestamp queries if possible:
    if (mTimestampSupported)
    {
        for (auto &res : mResources)
        {
            VkQueryPoolCreateInfo queryPoolInfo{};
            queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
            queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
            queryPoolInfo.queryCount = TimestampsPerFrame;

            auto queryRes = vkCreateQueryPool(mCtx.Device, &queryPoolInfo, nullptr,
                                              &res.TimestampQueryPool);

            vassert(queryRes == VK_SUCCESS, "Failed to create query pool!");

            mDeletionQueue.push_back(res.TimestampQueryPool);

            // Reset all timestamps preemptively
            mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
                vkCmdResetQueryPool(cmd, res.TimestampQueryPool, 0, TimestampsPerFrame);
            });
        }
    }

    // Setup statistics queries if possible:
    if (mPipelineStatisticsSupported)
    {
        for (auto &res : mResources)
        {
            VkQueryPipelineStatisticFlags queriedStatistics{};

            for (auto flag : QueriedStatistics)
                queriedStatistics |= flag;

            VkQueryPoolCreateInfo queryPoolInfo{};
            queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
            queryPoolInfo.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS;
            queryPoolInfo.pipelineStatistics = queriedStatistics;
            queryPoolInfo.queryCount = 1;

            auto queryRes = vkCreateQueryPool(mCtx.Device, &queryPoolInfo, nullptr,
                                              &res.StatisticsQueryPool);

            vassert(queryRes == VK_SUCCESS, "Failed to create query pool!");

            mDeletionQueue.push_back(res.StatisticsQueryPool);

            mCtx.ImmediateSubmitGraphics([&](VkCommandBuffer cmd) {
                vkCmdResetQueryPool(cmd, res.StatisticsQueryPool, 0, 1);
            });
        }
    }
}

VulkanStatisticsCollector::~VulkanStatisticsCollector()
{
    mDeletionQueue.flush();
}

StatisticsResult VulkanStatisticsCollector::QueryResults(uint32_t frameIdx)
{
    auto &res = mResources.at(frameIdx);
    auto &timestamps = res.Timestamps;

    StatisticsResult ret{};

    if (mTimestampSupported)
    {
        // Detect first run:
        bool firstRun = res.TimestampsFirstRun;
        res.TimestampsFirstRun = false;

        // Query for timestamp results from the previous run of this frame:
        if (!firstRun && mTimestampSupported)
        {
            auto queryRes = vkGetQueryPoolResults(
                mCtx.Device, res.TimestampQueryPool,
                0,                                        // first query
                static_cast<uint32_t>(timestamps.size()), // query count
                timestamps.size() * sizeof(Timestamp),    // data size
                &timestamps[0],                           // data
                sizeof(Timestamp),                        // stride
                VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);

            vassert(queryRes == VK_SUCCESS || queryRes == VK_NOT_READY);
        }

        // Check if the timestamps are ready:
        bool timestampsReady =
            (timestamps[0].Availability != 0) && (timestamps[1].Availability != 0);

        // Decide if new timestamps can be written:
        res.WriteTimestamps = firstRun || timestampsReady;

        // Store timestamp results if ready:
        if (timestampsReady)
        {
            auto diff = static_cast<float>(timestamps[1].Value - timestamps[0].Value);
            ret.FrameTimeMS =
                diff * mTimestampPeriod / 1e6f; // nanoseconds to miliseconds
        }
    }

    if (mPipelineStatisticsSupported)
    {
        auto queryRes = vkGetQueryPoolResults(
            mCtx.Device, res.StatisticsQueryPool, 0, 1,
            res.Statistics.size() * sizeof(Statistic), res.Statistics.data(),
            res.Statistics.size() * sizeof(Statistic),
            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);

        vassert(queryRes == VK_SUCCESS || queryRes == VK_NOT_READY);

        bool statsReady = res.Statistics.at(0).Availability != 0;

        if (statsReady)
        {
            ret.FragmentInvocations = res.Statistics.at(0).Value;
        }
    }

    return ret;
}

void VulkanStatisticsCollector::TimestampTop(VkCommandBuffer cmd, uint32_t frameIdx)
{
    auto &res = mResources.at(frameIdx);

    if (mTimestampSupported && res.WriteTimestamps)
    {
        vkCmdResetQueryPool(cmd, res.TimestampQueryPool, 0, 1);
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            res.TimestampQueryPool, 0);
    }
}

void VulkanStatisticsCollector::TimestampBottom(VkCommandBuffer cmd, uint32_t frameIdx)
{
    auto &res = mResources.at(frameIdx);

    if (mTimestampSupported && res.WriteTimestamps)
    {
        vkCmdResetQueryPool(cmd, res.TimestampQueryPool, 1, 1);
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                            res.TimestampQueryPool, 1);
    }
}

void VulkanStatisticsCollector::PipelineStatsStart(VkCommandBuffer cmd, uint32_t frameIdx)
{
    auto &res = mResources.at(frameIdx);

    if (mPipelineStatisticsSupported)
    {
        vkCmdResetQueryPool(cmd, res.StatisticsQueryPool, 0, 1);
        vkCmdBeginQuery(cmd, res.StatisticsQueryPool, 0, 0);
    }
}

void VulkanStatisticsCollector::PipelineStatsEnd(VkCommandBuffer cmd, uint32_t frameIdx)
{
    auto &res = mResources.at(frameIdx);

    if (mPipelineStatisticsSupported)
    {
        vkCmdEndQuery(cmd, res.StatisticsQueryPool, 0);
    }
}