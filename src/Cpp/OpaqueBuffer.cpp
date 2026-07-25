#include "OpaqueBuffer.h"
#include "Pch.h"

#include <new>

OpaqueBuffer::OpaqueBuffer(size_t size, size_t alignment) : Size(size)
{
#ifdef _WIN32
    Data = static_cast<uint8_t *>(_aligned_malloc(size, alignment));
#else
    Data = new (std::align_val_t(alignment)) uint8_t[size];
#endif
}

OpaqueBuffer::~OpaqueBuffer()
{
#ifdef _WIN32
    _aligned_free(Data);
#else
    delete[] Data;
#endif
}

OpaqueBuffer::OpaqueBuffer(OpaqueBuffer &&other) noexcept
    : Size(other.Size), Data(other.Data)
{
    other.Data = nullptr;
}

OpaqueBuffer &OpaqueBuffer::operator=(OpaqueBuffer &&other) noexcept
{
    Size = other.Size;
    Data = other.Data;

    other.Data = nullptr;

    return *this;
}