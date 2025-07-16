#pragma once

#include <string_view>
#include <source_location>

void vassert(bool condition, const std::source_location location = std::source_location::current());
void vassert(bool condition, std::string_view message, const std::source_location location = std::source_location::current());
void vpanic(std::string_view message, const std::source_location location = std::source_location::current());