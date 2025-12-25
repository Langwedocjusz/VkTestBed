#include "Vassert.h"
#include "Pch.h"

#include <cpptrace/cpptrace.hpp>

#include <cstdlib>
#include <iostream>

void vassert(bool condition, const std::source_location location)
{
    if (!condition)
    {
        std::cout << "ASSERTION FAILED\n";
        std::cout << "FILE: " << location.file_name() << '\n';
        std::cout << "LINE: " << location.line() << "\n\n";

        cpptrace::generate_trace().print();

        std::abort();
    }
}

void vassert(bool condition, std::string_view message,
             const std::source_location location)
{
    if (!condition)
    {
        std::cout << "ASSERTION FAILED\n";
        std::cout << "FILE: " << location.file_name() << '\n';
        std::cout << "LINE: " << location.line() << "\n\n";

        std::cout << message << "\n\n";

        cpptrace::generate_trace().print();

        std::abort();
    }
}

void vpanic(std::string_view message, const std::source_location location)
{
    std::cout << "PANIC TRIGGERED\n";
    std::cout << "FILE: " << location.file_name() << '\n';
    std::cout << "LINE: " << location.line() << "\n\n";

    std::cout << message << "\n\n";

    cpptrace::generate_trace().print();

    std::abort();
}