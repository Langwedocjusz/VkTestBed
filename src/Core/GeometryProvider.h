#pragma once

#include <functional>
#include <optional>
#include <variant>
#include <vector>

// Vertex data:

#include "Vertex.h"

template <Vertex V>
using VertexProviderFn = std::function<std::vector<V>()>;

// This needs to be a variant over all supported Vertex Types:
using VertexProviderVariant = std::variant<VertexProviderFn<ColoredVertex>>;

// Index data:
template <typename T>
concept ValidIndexType = std::same_as<T, uint16_t> || std::same_as<T, uint32_t>;

template <ValidIndexType I>
using IndexProviderFn = std::function<std::vector<I>()>;

using IndexProviderVariant =
    std::variant<IndexProviderFn<uint16_t>, IndexProviderFn<uint32_t>>;

// Geometry provider:
struct GeometryProvider {
    VertexProviderVariant Vert;
    IndexProviderVariant Idx;

    template <Vertex V>
    std::optional<VertexProviderFn<V>> UnpackVertices()
    {
        if (std::holds_alternative<VertexProviderFn<V>>(Vert))
            return std::get<VertexProviderFn<V>>(Vert);

        return {};
    }

    template <ValidIndexType I>
    std::optional<IndexProviderFn<I>> UnpackIndices()
    {
        if (std::holds_alternative<IndexProviderFn<I>>(Idx))
            return std::get<IndexProviderFn<I>>(Idx);

        return {};
    }
};