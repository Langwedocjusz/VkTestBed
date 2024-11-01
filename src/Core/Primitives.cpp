#include "Primitives.h"

#include "GeometryProvider.h"
#include "VertexLayout.h"

#include <cstdint>
#include <vulkan/vulkan.h>

// clang-format off
struct HelloVertex{
    glm::vec3 Position;
    glm::vec3 Color;
};

struct Vertex_PosTexCol{
    glm::vec3 Position;
    glm::vec2 TexCoord;
    glm::vec3 Color;
};

struct Vertex_PosColNorm{
    glm::vec3 Position;
    glm::vec3 Color;
    glm::vec3 Normal;
};

struct Vertex_PosTexNorm{
    glm::vec3 Position;
    glm::vec2 TexCoord;
    glm::vec3 Normal;
};

GeometryProvider primitive::HelloTriangle()
{
    using enum Vertex::AttributeType;

    GeometryLayout layout{
        .VertexLayout = {Vec3, Vec3},
        .IndexType = VK_INDEX_TYPE_UINT16
    };

    auto vertexProvider = []()
    {
        constexpr size_t vertexCount = 3;
        constexpr size_t size = vertexCount * sizeof(HelloVertex);

        OpaqueBuffer buf(vertexCount, size, 4);

        const float r3 = std::sqrt(3.0f);

        new (buf.Data.get()) HelloVertex[vertexCount]
        {
            {{ 0.0f,-r3/3.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            {{ 0.5f, r3/6.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            {{-0.5f, r3/6.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        };

        return buf;
    };

    auto indexProvider = []()
    {
        constexpr size_t idxCount = 3;
        constexpr size_t size = idxCount * sizeof(uint16_t);

        OpaqueBuffer buf(idxCount, size, 2);

        new (buf.Data.get()) uint16_t[idxCount]{0, 1, 2};

        return buf;
    };

    return GeometryProvider{
        layout,
        vertexProvider,
        indexProvider,
    };
}

GeometryProvider primitive::HelloQuad()
{
    using enum Vertex::AttributeType;

    GeometryLayout layout{
        .VertexLayout = {Vec3, Vec3},
        .IndexType = VK_INDEX_TYPE_UINT16
    };

    auto vertexProvider = []()
    {
        constexpr size_t vertexCount = 4;
        constexpr size_t size = vertexCount * sizeof(HelloVertex);

        OpaqueBuffer buf(vertexCount, size, 4);

        new (buf.Data.get()) HelloVertex[vertexCount]
        {
            {{-0.33f, 0.33f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            {{ 0.33f, 0.33f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            {{ 0.33f,-0.33f, 0.0f}, {0.0f, 0.0f, 1.0f}},
            {{-0.33f,-0.33f, 0.0f}, {1.0f, 1.0f, 1.0f}},
        };

        return buf;
    };

    auto indexProvider = []()
    {
        constexpr size_t idxCount = 6;
        constexpr size_t size = idxCount * sizeof(uint16_t);

        OpaqueBuffer buf(idxCount, size, 2);

        new (buf.Data.get()) uint16_t[idxCount]{0, 2, 1, 2, 0, 3};

        return buf;
    };

    return GeometryProvider{
        layout,
        vertexProvider,
        indexProvider,
    };
}

GeometryProvider primitive::TexturedQuad()
{
    using enum Vertex::AttributeType;

    GeometryLayout layout{
        .VertexLayout = {Vec3, Vec2, Vec3},
        .IndexType = VK_INDEX_TYPE_UINT32
    };

    auto vertexProvider = []()
    {
        constexpr size_t vertexCount = 4;
        constexpr size_t size = vertexCount * sizeof(Vertex_PosTexCol);

        OpaqueBuffer buf(vertexCount, size, 4);

        new (buf.Data.get()) Vertex_PosTexCol[vertexCount]
        {
            {{-0.5f,-0.5f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f,-1.0f}},
            {{ 0.5f,-0.5f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f,-1.0f}},
            {{ 0.5f, 0.5f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f,-1.0f}},
            {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f,-1.0f}},
        };

        return buf;
    };

    auto indexProvider = []()
    {
        constexpr size_t idxCount = 6;
        constexpr size_t size = idxCount * sizeof(uint32_t);

        OpaqueBuffer buf(idxCount, size, 4);

        new (buf.Data.get()) uint32_t[idxCount]{0, 1, 2, 2, 3, 0};

        return buf;
    };

    return GeometryProvider{
        layout,
        vertexProvider,
        indexProvider,
    };
}

GeometryProvider primitive::ColoredCube(glm::vec3 color)
{
    //To-do: actually use this:
    (void)color;

    using enum Vertex::AttributeType;

    GeometryLayout layout{
        .VertexLayout = {Vec3, Vec3, Vec3},
        .IndexType = VK_INDEX_TYPE_UINT32
    };

    auto vertexProvider = []()
    {
        constexpr size_t vertexCount = 24;
        constexpr size_t size = vertexCount * sizeof(Vertex_PosColNorm);

        OpaqueBuffer buf(vertexCount, size, 4);

        new (buf.Data.get()) Vertex_PosColNorm[vertexCount]
        {
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

        return buf;
    };

    auto indexProvider = []()
    {
        constexpr size_t idxCount = 36;
        constexpr size_t size = idxCount * sizeof(uint32_t);

        OpaqueBuffer buf(idxCount, size, 4);

        new (buf.Data.get()) uint32_t[idxCount]
        {
            //Top
            0,1,2, 2,3,0,
            //Bottom
            4,6,5, 6,4,7,
            //Front
            8,10,9, 10,8,11,
            //Back
            12,13,14, 14,15,12,
            //Right
            16,18,17, 18,16,19,
            //Left
            20,21,22, 22,23,20
        };

        return buf;
    };

    return GeometryProvider{
        layout,
        vertexProvider,
        indexProvider,
    };
}

GeometryProvider primitive::TexturedCube()
{
    using enum Vertex::AttributeType;

    GeometryLayout layout{
        .VertexLayout = {Vec3, Vec2, Vec3},
        .IndexType = VK_INDEX_TYPE_UINT32
    };

    auto vertexProvider = []()
    {
        constexpr size_t vertexCount = 24;
        constexpr size_t size = vertexCount * sizeof(Vertex_PosTexNorm);

        OpaqueBuffer buf(vertexCount, size, 4);

        new (buf.Data.get()) Vertex_PosTexNorm[vertexCount]
        {
            //Top
            {{-0.5f, 0.5f, 0.5f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            {{ 0.5f, 0.5f, 0.5f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
            {{ 0.5f, 0.5f,-0.5f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
            {{-0.5f, 0.5f,-0.5f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            //Bottom
            {{-0.5f,-0.5f, 0.5f}, {1.0f, 0.0f}, {0.0f,-1.0f, 0.0f}},
            {{ 0.5f,-0.5f, 0.5f}, {1.0f, 1.0f}, {0.0f,-1.0f, 0.0f}},
            {{ 0.5f,-0.5f,-0.5f}, {0.0f, 1.0f}, {0.0f,-1.0f, 0.0f}},
            {{-0.5f,-0.5f,-0.5f}, {0.0f, 0.0f}, {0.0f,-1.0f, 0.0f}},
            //Front
            {{-0.5f, 0.5f, 0.5f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
            {{ 0.5f, 0.5f, 0.5f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
            {{ 0.5f,-0.5f, 0.5f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
            {{-0.5f,-0.5f, 0.5f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
            //Back
            {{-0.5f, 0.5f,-0.5f}, {0.0f, 1.0f}, {0.0f, 0.0f,-1.0f}},
            {{ 0.5f, 0.5f,-0.5f}, {1.0f, 1.0f}, {0.0f, 0.0f,-1.0f}},
            {{ 0.5f,-0.5f,-0.5f}, {1.0f, 0.0f}, {0.0f, 0.0f,-1.0f}},
            {{-0.5f,-0.5f,-0.5f}, {0.0f, 0.0f}, {0.0f, 0.0f,-1.0f}},
            //Right
            {{ 0.5f,-0.5f, 0.5f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            {{ 0.5f, 0.5f, 0.5f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
            {{ 0.5f, 0.5f,-0.5f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
            {{ 0.5f,-0.5f,-0.5f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            //Left
            {{-0.5f,-0.5f, 0.5f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
            {{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
            {{-0.5f, 0.5f,-0.5f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
            {{-0.5f,-0.5f,-0.5f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
        };

        return buf;
    };

    auto indexProvider = []()
    {
        constexpr size_t idxCount = 36;
        constexpr size_t size = idxCount * sizeof(uint32_t);

        OpaqueBuffer buf(idxCount, size, 4);

        new (buf.Data.get()) uint32_t[idxCount]
        {
            //Top
            0,1,2, 2,3,0,
            //Bottom
            4,6,5, 6,4,7,
            //Front
            8,10,9, 10,8,11,
            //Back
            12,13,14, 14,15,12,
            //Right
            16,18,17, 18,16,19,
            //Left
            20,21,22, 22,23,20
        };

        return buf;
    };

    return GeometryProvider{
        layout,
        vertexProvider,
        indexProvider,
    };
}

// clang-format on