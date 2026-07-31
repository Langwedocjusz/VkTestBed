#pragma once

#include <source_location>
#include <string_view>

// Vassert uses two overloads instead of
// default value optional, since
// source location must already be supplied
// by default argument.

void vassert(bool                       condition,
             const std::source_location location = std::source_location::current());
void vassert(bool condition, std::string_view message,
             const std::source_location location = std::source_location::current());
void vpanic(std::string_view           message,
            const std::source_location location = std::source_location::current());