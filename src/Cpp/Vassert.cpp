#include "Vassert.h"
#include "Pch.h"

#include <cpptrace/cpptrace.hpp>

#include <cstdlib>
#include <iostream>
#include <mutex>
#include <optional>

static void ThreadSafeLog(std::string_view header, const std::source_location location, std::optional<std::string_view> message = std::nullopt)
{
    // Using local static to avoid initialization order fiasco:
    static std::mutex logMutex{};

    // Locking for the entire logging scope:
    std::lock_guard lock{logMutex};

    std::cout << header << '\n';
    std::cout << "FILE: " << location.file_name() << '\n';
    std::cout << "LINE: " << location.line() << "\n\n";

    if (message.has_value())
        std::cout << *message << "\n\n";

    cpptrace::generate_trace().print();
}

void vassert(bool condition, const std::source_location location)
{
    if (condition)
        return;

    ThreadSafeLog("ASSERTION FAILED", location);

    std::abort();
}

void vassert(bool condition, std::string_view message,
             const std::source_location location)
{
    if (condition)
        return;

    ThreadSafeLog("ASSERTION FAILED", location, message);

    std::abort();
}

void vpanic(std::string_view message, const std::source_location location)
{
    ThreadSafeLog("PANIC TRIGGERED", location, message);

    std::abort();
}