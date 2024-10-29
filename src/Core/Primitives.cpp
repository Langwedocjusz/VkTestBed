#include "Primitives.h"

#include "GeometryProvider.h"
#include "Vertex.h"

#include <cstdint>

static std::vector<Vertex_PC> HelloTriangleVertices()
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

static std::vector<Vertex_PC> HelloQuadVertices()
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

GeometryProvider primitive::TexturedQuad()
{
    // clang-format off
    auto vertices = []() -> std::vector<Vertex_PXN>
    {
        return {
            {{-0.5f,-0.5f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f,-1.0f}},
            {{ 0.5f,-0.5f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f,-1.0f}},
            {{ 0.5f, 0.5f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f,-1.0f}},
            {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f,-1.0f}}
        };
    };

    auto indices = []() -> std::vector<uint32_t>
    {
        return {
            0, 1, 2, 2, 3, 0
        };
    };

    // clang-format on

    return GeometryProvider{
        vertices,
        indices
    };
}

GeometryProvider primitive::ColoredCube(glm::vec3 color)
{
    (void)color;

    // clang-format off
    auto vertices = []() -> std::vector<Vertex_PCN>
    {
        return {
            //Top
            {{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            {{ 0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            {{ 0.5f, 0.5f,-0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            {{-0.5f, 0.5f,-0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            //Bottom
            {{-0.5f,-0.5f, 0.5f}, {1.0f, 0.0f, 1.0f}, {0.0f,-1.0f, 0.0f}},
            {{ 0.5f,-0.5f, 0.5f}, {1.0f, 0.0f, 1.0f}, {0.0f,-1.0f, 0.0f}},
            {{ 0.5f,-0.5f,-0.5f}, {1.0f, 0.0f, 1.0f}, {0.0f,-1.0f, 0.0f}},
            {{-0.5f,-0.5f,-0.5f}, {1.0f, 0.0f, 1.0f}, {0.0f,-1.0f, 0.0f}},
            //Front
            {{-0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
            {{ 0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
            {{ 0.5f,-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
            {{-0.5f,-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
            //Back
            {{-0.5f, 0.5f,-0.5f}, {1.0f, 1.0f, 0.0f}, {0.0f, 0.0f,-1.0f}},
            {{ 0.5f, 0.5f,-0.5f}, {1.0f, 1.0f, 0.0f}, {0.0f, 0.0f,-1.0f}},
            {{ 0.5f,-0.5f,-0.5f}, {1.0f, 1.0f, 0.0f}, {0.0f, 0.0f,-1.0f}},
            {{-0.5f,-0.5f,-0.5f}, {1.0f, 1.0f, 0.0f}, {0.0f, 0.0f,-1.0f}},
            //Right
            {{ 0.5f,-0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            {{ 0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            {{ 0.5f, 0.5f,-0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            {{ 0.5f,-0.5f,-0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            //Left
            {{-0.5f,-0.5f, 0.5f}, {0.0f, 1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
            {{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
            {{-0.5f, 0.5f,-0.5f}, {0.0f, 1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
            {{-0.5f,-0.5f,-0.5f}, {0.0f, 1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
        };
    };

    auto indices = []() -> std::vector<uint32_t>
    {
        return {
            //Top
            //0,1,2, 2,3,0,
            0,2,1, 2,0,3,
            //Bottom
            4,5,6, 6,7,4,
            //Front
            8,9,10, 10,11,8,
            //Back
            //12,13,14, 14,15,12,
            12,14,13, 14,12,15,
            //Right
            16,17,18, 18,19,16,
            //Left
            //20,21,22, 22,23,20
            20,22,21, 22,20,23
        };
    };
    // clang-format on

    return GeometryProvider{
        vertices,
        indices
    };
}