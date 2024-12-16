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
        std::vector<GeometryData> res;

        constexpr auto spec = GeometrySpec::BuildV<HelloVertex, uint16_t>(3, 3);

        auto& geo = res.emplace_back(spec);

        const float r3 = std::sqrt(3.0f);

        new (geo.VertexData.Data.get()) HelloVertex[spec.VertCount]
        {
            {{ 0.0f,-r3/3.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            {{ 0.5f, r3/6.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            {{-0.5f, r3/6.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        };

        new (geo.IndexData.Data.get()) uint16_t[spec.IdxCount]{0, 1, 2};

        return res;
    };

    return GeometryProvider{
        //"Hello Triangle",
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
        std::vector<GeometryData> res;

        constexpr auto spec = GeometrySpec::BuildV<HelloVertex, uint16_t>(4, 6);

        auto& geo = res.emplace_back(spec);

        new (geo.VertexData.Data.get()) HelloVertex[spec.VertCount]
        {
            {{-0.33f, 0.33f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            {{ 0.33f, 0.33f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            {{ 0.33f,-0.33f, 0.0f}, {0.0f, 0.0f, 1.0f}},
            {{-0.33f,-0.33f, 0.0f}, {1.0f, 1.0f, 1.0f}},
        };

        new (geo.IndexData.Data.get()) uint16_t[spec.IdxCount]{0, 2, 1, 2, 0, 3};

        return res;
    };

    return GeometryProvider{
        //"Hello Quad",
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
        std::vector<GeometryData> res;

        constexpr auto spec = GeometrySpec::BuildV<HelloVertex, uint32_t>(4, 6);

        auto& geo = res.emplace_back(spec);

        new (geo.VertexData.Data.get()) Vertex_PosTexCol[spec.VertCount]
        {
            {{-0.5f,-0.5f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f,-1.0f}},
            {{ 0.5f,-0.5f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f,-1.0f}},
            {{ 0.5f, 0.5f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f,-1.0f}},
            {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f,-1.0f}},
        };

        new (geo.IndexData.Data.get()) uint32_t[spec.IdxCount]{0, 1, 2, 2, 3, 0};

        return res;
    };

    return GeometryProvider{
        //"Textured Quad",
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
        std::vector<GeometryData> res;

        constexpr auto spec = GeometrySpec::BuildV<Vertex_PosColNorm, uint32_t>(24, 36);

        auto& geo = res.emplace_back(spec);

        new (geo.VertexData.Data.get()) Vertex_PosColNorm[spec.VertCount]
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

        new (geo.IndexData.Data.get()) uint32_t[spec.IdxCount]
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

        return res;
    };

    return GeometryProvider{
        //"Colored Cube",
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
        std::vector<GeometryData> res;

        constexpr auto spec = GeometrySpec::BuildV<Vertex_PosTexNorm, uint32_t>(24, 36);

        auto& geo = res.emplace_back(spec);

        new (geo.VertexData.Data.get()) Vertex_PosTexNorm[spec.VertCount]
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

        new (geo.IndexData.Data.get()) uint32_t[spec.IdxCount]
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

        return res;
    };

    return GeometryProvider{
        //"Textured Cube",
        layout,
        geoProvider,
    };
}

// clang-format on