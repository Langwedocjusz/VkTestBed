#include "Primitives.h"

#include "Vertex.h"

#include <cstdint>

struct HelloTriangleVertexProvider : public VertexProvider<ColoredVertex> {
    std::vector<ColoredVertex> GetVertices() override
    {
        const float r3 = std::sqrt(3.0f);

        return {
            {{0.0f, -r3 / 3.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            {{0.5f, r3 / 6.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            {{-0.5f, r3 / 6.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        };
    }
};

struct HelloTriangleIndexProvider : public IndexProvider<uint16_t> {
    std::vector<uint16_t> GetIndices() override
    {
        return {0, 1, 2};
    }
};

GeometryProvider primitive::HelloTriangle()
{
    return GeometryProvider{
        std::make_unique<HelloTriangleVertexProvider>(),
        std::make_unique<HelloTriangleIndexProvider>(),
    };
}

struct HelloQuadVertexProvider : public VertexProvider<ColoredVertex> {
    std::vector<ColoredVertex> GetVertices() override
    {
        return {
            {{-0.33f, 0.33f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            {{0.33f, 0.33f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            {{0.33f, -0.33f, 0.0f}, {0.0f, 0.0f, 1.0f}},
            {{-0.33f, -0.33f, 0.0f}, {1.0f, 1.0f, 1.0f}},
        };
    }
};

struct HelloQuadIndexProvider : public IndexProvider<uint16_t> {
    std::vector<uint16_t> GetIndices() override
    {
        return {0, 2, 1, 2, 0, 3};
    }
};

GeometryProvider primitive::HelloQuad()
{
    return GeometryProvider{
        std::make_unique<HelloQuadVertexProvider>(),
        std::make_unique<HelloQuadIndexProvider>(),
    };
}