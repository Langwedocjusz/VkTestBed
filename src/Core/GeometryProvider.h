#pragma once

#include "VertexLayout.h"

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

struct OpaqueBuffer {
    size_t Count;
    size_t Size;
    std::unique_ptr<uint8_t> Data;

    OpaqueBuffer(size_t count, size_t size, size_t alignment) : Count(count), Size(size)
    {
        Data = std::unique_ptr<uint8_t>(new (std::align_val_t(alignment)) uint8_t[size]);
    }
};

struct GeometryData {
    OpaqueBuffer VertexData;
    OpaqueBuffer IndexData;
};

struct GeometryProvider {
    std::string Name;
    GeometryLayout Layout;
    std::function<GeometryData()> GetGeometry;
};