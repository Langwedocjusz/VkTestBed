#pragma once

#include <cstddef>

struct OpaqueBuffer {
    OpaqueBuffer() = default;
    OpaqueBuffer(size_t size, size_t alignment);

    ~OpaqueBuffer();

    OpaqueBuffer(const OpaqueBuffer &)            = delete;
    OpaqueBuffer &operator=(const OpaqueBuffer &) = delete;

    OpaqueBuffer(OpaqueBuffer &&other) noexcept;
    OpaqueBuffer &operator=(OpaqueBuffer &&other) noexcept;

    size_t     Size = 0;
    std::byte *Data = nullptr;
private:
    void Deallocate();
};