#pragma once

#include "VertexLayout.h"

#include "CppUtils.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <new>

struct GeometryLayout {
    Vertex::Layout VertexLayout;
    VkIndexType IndexType;

    bool IsCompatible(const GeometryLayout &other)
    {
        bool idxCompat = IndexType == other.IndexType;
        // To-do: this will be weakened in the future:
        bool vertCompat = VertexLayout == other.VertexLayout;

        return idxCompat && vertCompat;
    }
};

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

    OpaqueBuffer(size_t count, size_t size, size_t alignment)
        : Count(count), Size(size)
    {
        #ifdef _MSC_VER 
        uint8_t *ptr = static_cast<uint8_t*>(_aligned_malloc(size, alignment));
        Data = std::unique_ptr<uint8_t, MSVCAlignedDeleter<uint8_t>>(ptr);
        #else
        uint8_t *ptr = new (std::align_val_t(alignment)) uint8_t[size];
        Data = std::unique_ptr<uint8_t>(ptr);
        #endif
    }
};

struct GeometrySpec {
    size_t VertCount = 0;
    size_t VertBuffSize = 0;
    size_t VertAlignment = 4;
    size_t IdxCount = 0;
    size_t IdxBuffSize = 0;
    size_t IdxAlignment = 4;

    // This assumes vertex alignment is equal to 4
    // which is typically the case.
    template <typename VertType, typename IdxType>
    static constexpr GeometrySpec Build(size_t vertCount, size_t idxCount)
    {
        static_assert(std::is_same<IdxType, uint16_t>::value ||
                      std::is_same<IdxType, uint32_t>::value);

        constexpr auto idxAlignment = []() -> size_t {
            if constexpr (std::is_same<IdxType, uint16_t>::value)
                return 2;
            else
                return 4;
        }();

        return GeometrySpec{
            .VertCount = vertCount,
            .VertBuffSize = vertCount * sizeof(VertType),
            .VertAlignment = 4,
            .IdxCount = idxCount,
            .IdxBuffSize = idxCount * sizeof(IdxType),
            .IdxAlignment = idxAlignment,
        };
    }
};

struct GeometryData {
    GeometryData(const GeometrySpec &spec)
        : VertexData(spec.VertCount, spec.VertBuffSize, spec.VertAlignment),
          IndexData(spec.IdxCount, spec.IdxBuffSize, spec.IdxAlignment)
    {
    }

    OpaqueBuffer VertexData;
    OpaqueBuffer IndexData;
};

struct GeometryProvider {
    GeometryLayout Layout;
    std::function<std::vector<GeometryData>()> GetGeometry;
};