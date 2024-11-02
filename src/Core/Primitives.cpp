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

    auto geoProvider = []()
    {
        constexpr size_t vertCount = 3;
        constexpr size_t vertSize = vertCount * sizeof(HelloVertex);

        OpaqueBuffer vertBuf(vertCount, vertSize, 4);

        const float r3 = std::sqrt(3.0f);

        new (vertBuf.Data.get()) HelloVertex[vertCount]
        {
            {{ 0.0f,-r3/3.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            {{ 0.5f, r3/6.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            {{-0.5f, r3/6.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        };

        constexpr size_t idxCount = 3;
        constexpr size_t size = idxCount * sizeof(uint16_t);

        OpaqueBuffer idxBuf(idxCount, size, 2);

        new (idxBuf.Data.get()) uint16_t[idxCount]{0, 1, 2};

        return GeometryData{
            std::move(vertBuf), std::move(idxBuf)
        };
    };

    return GeometryProvider{
        layout,
        geoProvider,
    };
}

GeometryProvider primitive::HelloQuad()
{
    using enum Vertex::AttributeType;

    GeometryLayout layout{
        .VertexLayout = {Vec3, Vec3},
        .IndexType = VK_INDEX_TYPE_UINT16
    };

    auto geoProvider = []()
    {
        constexpr size_t vertCount = 4;
        constexpr size_t vertSize = vertCount * sizeof(HelloVertex);

        OpaqueBuffer vertBuf(vertCount, vertSize, 4);

        new (vertBuf.Data.get()) HelloVertex[vertCount]
        {
            {{-0.33f, 0.33f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            {{ 0.33f, 0.33f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            {{ 0.33f,-0.33f, 0.0f}, {0.0f, 0.0f, 1.0f}},
            {{-0.33f,-0.33f, 0.0f}, {1.0f, 1.0f, 1.0f}},
        };

        constexpr size_t idxCount = 6;
        constexpr size_t idxSize = idxCount * sizeof(uint16_t);

        OpaqueBuffer idxBuf(idxCount, idxSize, 2);

        new (idxBuf.Data.get()) uint16_t[idxCount]{0, 2, 1, 2, 0, 3};

        return GeometryData{
            std::move(vertBuf), std::move(idxBuf)
        };
    };

    return GeometryProvider{
        layout,
        geoProvider,
    };
}

GeometryProvider primitive::TexturedQuad()
{
    using enum Vertex::AttributeType;

    GeometryLayout layout{
        .VertexLayout = {Vec3, Vec2, Vec3},
        .IndexType = VK_INDEX_TYPE_UINT32
    };

    auto geoProvider = []()
    {
        constexpr size_t vertCount = 4;
        constexpr size_t vertSize = vertCount * sizeof(Vertex_PosTexCol);

        OpaqueBuffer vertBuf(vertCount, vertSize, 4);

        new (vertBuf.Data.get()) Vertex_PosTexCol[vertCount]
        {
            {{-0.5f,-0.5f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f,-1.0f}},
            {{ 0.5f,-0.5f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f,-1.0f}},
            {{ 0.5f, 0.5f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f,-1.0f}},
            {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f,-1.0f}},
        };

        constexpr size_t idxCount = 6;
        constexpr size_t idxSize = idxCount * sizeof(uint32_t);

        OpaqueBuffer idxBuf(idxCount, idxSize, 4);

        new (idxBuf.Data.get()) uint32_t[idxCount]{0, 1, 2, 2, 3, 0};

        return GeometryData{
            std::move(vertBuf), std::move(idxBuf)
        };
    };

    return GeometryProvider{
        layout,
        geoProvider,
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

    auto geoProvider = []()
    {
        constexpr size_t vertCount = 24;
        constexpr size_t vertSize = vertCount * sizeof(Vertex_PosColNorm);

        OpaqueBuffer vertBuf(vertCount, vertSize, 4);

        new (vertBuf.Data.get()) Vertex_PosColNorm[vertCount]
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

        constexpr size_t idxCount = 36;
        constexpr size_t idxSize = idxCount * sizeof(uint32_t);

        OpaqueBuffer idxBuf(idxCount, idxSize, 4);

        new (idxBuf.Data.get()) uint32_t[idxCount]
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

        return GeometryData{
            std::move(vertBuf), std::move(idxBuf)
        };
    };

    return GeometryProvider{
        layout,
        geoProvider,
    };
}

GeometryProvider primitive::TexturedCube()
{
    using enum Vertex::AttributeType;

    GeometryLayout layout{
        .VertexLayout = {Vec3, Vec2, Vec3},
        .IndexType = VK_INDEX_TYPE_UINT32
    };

    auto geoProvider = []()
    {
        constexpr size_t vertCount = 24;
        constexpr size_t vertSize = vertCount * sizeof(Vertex_PosTexNorm);

        OpaqueBuffer vertBuf(vertCount, vertSize, 4);

        new (vertBuf.Data.get()) Vertex_PosTexNorm[vertCount]
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

        constexpr size_t idxCount = 36;
        constexpr size_t idxSize = idxCount * sizeof(uint32_t);

        OpaqueBuffer idxBuf(idxCount, idxSize, 4);

        new (idxBuf.Data.get()) uint32_t[idxCount]
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

        return GeometryData{
            std::move(vertBuf), std::move(idxBuf)
        };
    };

    return GeometryProvider{
        layout,
        geoProvider,
    };
}

// clang-format on