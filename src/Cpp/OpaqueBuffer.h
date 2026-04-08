#pragma once

#include <cstddef>
#include <cstdint>

struct OpaqueBuffer {
    OpaqueBuffer() = default;
    OpaqueBuffer(size_t size, size_t alignment);

    ~OpaqueBuffer();

    OpaqueBuffer(const OpaqueBuffer &)            = delete;
    OpaqueBuffer &operator=(const OpaqueBuffer &) = delete;

    OpaqueBuffer(OpaqueBuffer &&other) noexcept;
    OpaqueBuffer &operator=(OpaqueBuffer &&other) noexcept;

    size_t   Size = 0;
    uint8_t *Data = nullptr;
};