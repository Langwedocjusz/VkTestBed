#pragma once

#include <chrono>

namespace Timer
{
using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;

inline TimePoint Now()
{
    return std::chrono::high_resolution_clock::now();
}

inline float GetDiffMili(TimePoint current, TimePoint start)
{
    using ms = std::chrono::duration<float, std::milli>;

    return std::chrono::duration_cast<ms>(current - start).count();
}

inline float GetDiffSeconds(TimePoint current, TimePoint start)
{
    return GetDiffMili(current, start) / 1000.0f;
}
} // namespace Timer