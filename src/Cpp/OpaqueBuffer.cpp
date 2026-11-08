#include "OpaqueBuffer.h"
#include "Pch.h"

#include <new>

OpaqueBuffer::OpaqueBuffer(size_t size, size_t alignment)
    : Size(size)
{
#ifdef _WIN32
    Data = static_cast<std::byte *>(_aligned_malloc(size, alignment));
#else
    Data = new (std::align_val_t(alignment)) std::byte[size];
#endif
}

void OpaqueBuffer::Deallocate()
{
#ifdef _WIN32
    _aligned_free(Data);
#else
    delete[] Data;
#endif
}

OpaqueBuffer::~OpaqueBuffer()
{
    Deallocate();
}

OpaqueBuffer::OpaqueBuffer(OpaqueBuffer &&other) noexcept
    : Size(other.Size)
    , Data(other.Data)
{
    other.Data = {};
    other.Size = {};
}

OpaqueBuffer &OpaqueBuffer::operator=(OpaqueBuffer &&other) noexcept
{
    if (this != &other)
    {
        if (Data != nullptr)
        {
            Deallocate();
        }

        Size = other.Size;
        Data = other.Data;

        other.Data = {};
        other.Size = {};
    }

    return *this;
}