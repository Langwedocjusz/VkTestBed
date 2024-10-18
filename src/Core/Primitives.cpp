#include "Primitives.h"

std::vector<ColoredVertex> HelloTriangleVertexProvvider::GetVertices()
{
    const float r3 = std::sqrt(3.0f);

    return {
        {{0.0f, -r3 / 3.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, r3 / 6.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{-0.5f, r3 / 6.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
    };
}

std::vector<uint16_t> HelloTriangleIndexProvider::GetIndices()
{
    return {0, 1, 2};
}