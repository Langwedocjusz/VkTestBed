#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>

#ifdef _MSC_VER
template <class T>
struct MSVCAlignedDeleter {
    void operator()(T *ptr) const
    {
        ptr->~T();
        _aligned_free(ptr);
    }
};
#endif

struct OpaqueBuffer {
    size_t Count;
    size_t Size;

#ifdef _MSC_VER
    std::unique_ptr<uint8_t, MSVCAlignedDeleter<uint8_t>> Data;
#else
    std::unique_ptr<uint8_t> Data;
#endif

    OpaqueBuffer(size_t count, size_t size, size_t alignment) : Count(count), Size(size)
    {
#ifdef _MSC_VER
        auto *ptr = static_cast<uint8_t *>(_aligned_malloc(size, alignment));
        Data = std::unique_ptr<uint8_t, MSVCAlignedDeleter<uint8_t>>(ptr);
#else
        auto *ptr = new (std::align_val_t(alignment)) uint8_t[size];
        Data = std::unique_ptr<uint8_t>(ptr);
#endif
    }
};