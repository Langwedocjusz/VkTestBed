#pragma once

#include <chrono>

namespace Timer
{
using Point = std::chrono::time_point<std::chrono::high_resolution_clock>;

inline Point Get()
{
    return std::chrono::high_resolution_clock::now();
}

inline float GetDiffMili(Point start)
{
    using ms = std::chrono::duration<float, std::milli>;

    auto current = Get();
    return std::chrono::duration_cast<ms>(current - start).count();
}

inline float GetDiffSeconds(Point start)
{
    return GetDiffMili(start) / 1000.0f;
}
} // namespace Timer