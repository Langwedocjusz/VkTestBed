#include "Primitives.h"

#include "Vertex.h"

#include <cstdint>

static std::vector<ColoredVertex> HelloTriangleVertices()
{
    const float r3 = std::sqrt(3.0f);

    // clang-format off
    return {
        {{ 0.0f,-r3/3.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f, r3/6.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{-0.5f, r3/6.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
    };
    // clang-format on
}

static std::vector<uint16_t> HelloTriangleIndices()
{
    return {0, 1, 2};
}

GeometryProvider primitive::HelloTriangle()
{
    return GeometryProvider{
        HelloTriangleVertices,
        HelloTriangleIndices,
    };
}

static std::vector<ColoredVertex> HelloQuadVertices()
{
    // clang-format off
    return {
        {{-0.33f, 0.33f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.33f, 0.33f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{ 0.33f,-0.33f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{-0.33f,-0.33f, 0.0f}, {1.0f, 1.0f, 1.0f}},
    };
    // clang-format on
}

static std::vector<uint16_t> HelloQuadIndices()
{
    return {0, 2, 1, 2, 0, 3};
}

GeometryProvider primitive::HelloQuad()
{
    return GeometryProvider{
        HelloQuadVertices,
        HelloQuadIndices,
    };
}