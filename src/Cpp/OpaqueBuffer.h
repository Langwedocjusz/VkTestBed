#pragma once

#include <cstddef>
#include <cstdint>
#include <new>

struct OpaqueBuffer{
    size_t Count = 0;
    size_t Size = 0;    
    uint8_t* Data = nullptr;

    OpaqueBuffer() = default;
    
    OpaqueBuffer(size_t count, size_t size, size_t alignment) 
        : Count(count), Size(size)
    {
        #ifdef _MSC_VER
        Data = static_cast<uint8_t *>(_aligned_malloc(size, alignment));
        #else
        Data = new (std::align_val_t(alignment)) uint8_t[size];
        #endif
    }

    OpaqueBuffer(const OpaqueBuffer &) = delete;
    OpaqueBuffer &operator=(const OpaqueBuffer &) = delete;

    OpaqueBuffer(OpaqueBuffer &&other) noexcept
        :Count(other.Count), Size(other.Size), Data(other.Data)
    {
        other.Data = nullptr;
    }

    OpaqueBuffer &operator=(OpaqueBuffer &&other) noexcept
    {
        Count = other.Count;
        Size = other.Size;
        Data = other.Data;

        other.Data = nullptr;

        return *this;
    }

    ~OpaqueBuffer()
    {
        #ifdef _MSC_VER
        _aligned_free(Data);
        #else
        delete[] Data;
        #endif
    }
};