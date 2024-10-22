#pragma once

#include <memory>
#include <variant>
#include <vector>

// Vertex data:

#include "Vertex.h"

template <Vertex V>
struct VertexProvider {
    virtual ~VertexProvider() = default;
    virtual std::vector<V> GetVertices() = 0;
};

template <Vertex V>
using VertexProviderPtr = std::unique_ptr<VertexProvider<V>>;

using VertexProviderVariant = std::variant<VertexProviderPtr<ColoredVertex>>;

// Index data:
template <typename T>
concept ValidIndexType = std::same_as<T, uint16_t> || std::same_as<T, uint32_t>;

template <ValidIndexType I>
struct IndexProvider {
    virtual ~IndexProvider() = default;
    virtual std::vector<I> GetIndices() = 0;
};

template <ValidIndexType I>
using IndexProviderPtr = std::unique_ptr<IndexProvider<I>>;

using IndexProviderVariant =
    std::variant<IndexProviderPtr<uint16_t>, IndexProviderPtr<uint32_t>>;

// Geometry provider

struct GeometryProvider {
    VertexProviderVariant Vert;
    IndexProviderVariant Idx;

    template <Vertex V>
    VertexProviderPtr<V> UnpackVertices()
    {
        if (std::holds_alternative<VertexProviderPtr<V>>(Vert))
            return std::move(std::get<VertexProviderPtr<V>>(Vert));

        return nullptr;
    }

    template <ValidIndexType I>
    IndexProviderPtr<I> UnpackIndices()
    {
        if (std::holds_alternative<IndexProviderPtr<I>>(Idx))
            return std::move(std::get<IndexProviderPtr<I>>(Idx));

        return nullptr;
    }
};