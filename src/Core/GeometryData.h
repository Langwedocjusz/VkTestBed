#pragma once

#include "OpaqueBuffer.h"
#include "VertexLayout.h"

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

struct GeometrySpec {
    size_t VertCount = 0;
    size_t VertBuffSize = 0;
    size_t VertAlignment = 4;
    size_t IdxCount = 0;
    size_t IdxBuffSize = 0;
    size_t IdxAlignment = 4;

    // Builds geometry spec based on vertex size and index type.
    // Assumes vertex alignment is equal to 4.
    template <typename IdxType>
    static constexpr GeometrySpec BuildS(size_t vertSize, size_t vertCount,
                                         size_t idxCount)
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
            .VertBuffSize = vertCount * vertSize,
            .VertAlignment = 4,
            .IdxCount = idxCount,
            .IdxBuffSize = idxCount * sizeof(IdxType),
            .IdxAlignment = idxAlignment,
        };
    }

    // Builds geometry spec based on vertex and index types.
    // Assumes vertex alignment is equal to 4.
    template <typename VertType, typename IdxType>
    static constexpr GeometrySpec BuildV(size_t vertCount, size_t idxCount)
    {
        return BuildS<IdxType>(sizeof(VertType), vertCount, idxCount);
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

    GeometryLayout Layout;
};