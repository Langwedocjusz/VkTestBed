#include "Primitives.h"
#include "GeometryData.h"
#include "TangentsGenerator.h"

#include <cstdint>
#include <pthread.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

// clang-format off

GeometryData primitive::HelloTriangle()
{
    using enum Vertex::AttributeType;

    struct Vertex{
        glm::vec3 Position;
        glm::vec3 Color;
    };

    constexpr auto spec = GeometrySpec::BuildV<Vertex, uint16_t>(3, 3);

    GeometryData res(spec);

    const float r3 = std::sqrt(3.0f);

    new (res.VertexData.Data.get()) Vertex[spec.VertCount]
    {
        {{ 0.0f,-r3/3.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f, r3/6.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{-0.5f, r3/6.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
    };

    new (res.IndexData.Data.get()) uint16_t[spec.IdxCount]{0, 1, 2};

    res.Layout = GeometryLayout{
        .VertexLayout = {Vec3, Vec3},
        .IndexType = VK_INDEX_TYPE_UINT16
    };

    return res;
}

GeometryData primitive::HelloQuad()

{
    using enum Vertex::AttributeType;

    struct Vertex{
        glm::vec3 Position;
        glm::vec3 Color;
    };

    constexpr auto spec = GeometrySpec::BuildV<Vertex, uint16_t>(4, 6);

    GeometryData res(spec);

    new (res.VertexData.Data.get()) Vertex[spec.VertCount]
    {
        {{-0.33f, 0.33f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.33f, 0.33f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{ 0.33f,-0.33f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{-0.33f,-0.33f, 0.0f}, {1.0f, 1.0f, 1.0f}},
    };

    new (res.IndexData.Data.get()) uint16_t[spec.IdxCount]{0, 2, 1, 2, 0, 3};

    res.Layout = GeometryLayout{
        .VertexLayout = {Vec3, Vec3},
        .IndexType = VK_INDEX_TYPE_UINT16
    };

    return res;
}

GeometryData primitive::TexturedQuad()
{
    using enum Vertex::AttributeType;

    struct Vertex{
        glm::vec3 Position;
        glm::vec2 TexCoord;
        glm::vec3 Color;
    };

    constexpr auto spec = GeometrySpec::BuildV<Vertex, uint32_t>(4, 6);

    GeometryData res(spec);

    new (res.VertexData.Data.get()) Vertex[spec.VertCount]
    {
        {{-0.5f,-0.5f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f,-1.0f}},
        {{ 0.5f,-0.5f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f,-1.0f}},
        {{ 0.5f, 0.5f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f,-1.0f}},
        {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f,-1.0f}},
    };

    new (res.IndexData.Data.get()) uint32_t[spec.IdxCount]{0, 1, 2, 2, 3, 0};

    res.Layout = GeometryLayout{
        .VertexLayout = {Vec3, Vec2, Vec3},
        .IndexType = VK_INDEX_TYPE_UINT32
    };

    return res;
}

GeometryData primitive::ColoredCube()
{
    using enum Vertex::AttributeType;

    struct Vertex{
        glm::vec3 Position;
        glm::vec3 Color;
        glm::vec3 Normal;
    };

    constexpr auto spec = GeometrySpec::BuildV<Vertex, uint32_t>(24, 36);

    GeometryData res(spec);

    new (res.VertexData.Data.get()) Vertex[spec.VertCount]
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

    new (res.IndexData.Data.get()) uint32_t[spec.IdxCount]
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

    res.Layout = GeometryLayout{
        .VertexLayout = {Vec3, Vec3, Vec3},
        .IndexType = VK_INDEX_TYPE_UINT32
    };

    return res;
}

static GeometryData TexturedCubeImpl(bool withTangents)
{
    using enum Vertex::AttributeType;

    struct Vertex{
        glm::vec3 Position;
        glm::vec2 TexCoord;
        glm::vec3 Normal;
    };

    struct VertexT{
        glm::vec3 Position;
        glm::vec2 TexCoord;
        glm::vec3 Normal;
        glm::vec4 Tangent;
    };

    //Generate the geometry data object:
    const auto spec = [&](){
        if (withTangents)
            return GeometrySpec::BuildV<VertexT, uint32_t>(24, 36);
        else
        return GeometrySpec::BuildV<Vertex, uint32_t>(24, 36);
    }();

    GeometryData res(spec);

    //Provide vertex data:
    if (withTangents)
    {
        new (res.VertexData.Data.get()) VertexT[spec.VertCount]
        {
            //Top
            {{-0.5f, 0.5f, 0.5f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, glm::vec4(0.0f)},
            {{ 0.5f, 0.5f, 0.5f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, glm::vec4(0.0f)},
            {{ 0.5f, 0.5f,-0.5f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, glm::vec4(0.0f)},
            {{-0.5f, 0.5f,-0.5f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, glm::vec4(0.0f)},
            //Bottom
            {{-0.5f,-0.5f, 0.5f}, {1.0f, 0.0f}, {0.0f,-1.0f, 0.0f}, glm::vec4(0.0f)},
            {{ 0.5f,-0.5f, 0.5f}, {1.0f, 1.0f}, {0.0f,-1.0f, 0.0f}, glm::vec4(0.0f)},
            {{ 0.5f,-0.5f,-0.5f}, {0.0f, 1.0f}, {0.0f,-1.0f, 0.0f}, glm::vec4(0.0f)},
            {{-0.5f,-0.5f,-0.5f}, {0.0f, 0.0f}, {0.0f,-1.0f, 0.0f}, glm::vec4(0.0f)},
            //Front
            {{-0.5f, 0.5f, 0.5f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, glm::vec4(0.0f)},
            {{ 0.5f, 0.5f, 0.5f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, glm::vec4(0.0f)},
            {{ 0.5f,-0.5f, 0.5f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, glm::vec4(0.0f)},
            {{-0.5f,-0.5f, 0.5f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, glm::vec4(0.0f)},
            //Back
            {{-0.5f, 0.5f,-0.5f}, {0.0f, 1.0f}, {0.0f, 0.0f,-1.0f}, glm::vec4(0.0f)},
            {{ 0.5f, 0.5f,-0.5f}, {1.0f, 1.0f}, {0.0f, 0.0f,-1.0f}, glm::vec4(0.0f)},
            {{ 0.5f,-0.5f,-0.5f}, {1.0f, 0.0f}, {0.0f, 0.0f,-1.0f}, glm::vec4(0.0f)},
            {{-0.5f,-0.5f,-0.5f}, {0.0f, 0.0f}, {0.0f, 0.0f,-1.0f}, glm::vec4(0.0f)},
            //Right
            {{ 0.5f,-0.5f, 0.5f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, glm::vec4(0.0f)},
            {{ 0.5f, 0.5f, 0.5f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, glm::vec4(0.0f)},
            {{ 0.5f, 0.5f,-0.5f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, glm::vec4(0.0f)},
            {{ 0.5f,-0.5f,-0.5f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, glm::vec4(0.0f)},
            //Left
            {{-0.5f,-0.5f, 0.5f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, glm::vec4(0.0f)},
            {{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, glm::vec4(0.0f)},
            {{-0.5f, 0.5f,-0.5f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, glm::vec4(0.0f)},
            {{-0.5f,-0.5f,-0.5f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, glm::vec4(0.0f)}
        };
    }

    else 
    {
        new (res.VertexData.Data.get()) Vertex[spec.VertCount]
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
    }

    //Provide index data:
    new (res.IndexData.Data.get()) uint32_t[spec.IdxCount]
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

    //Generate the tangents if necessary:
    if (withTangents)
    {
        //Generate the tangents:
        auto layout = tangen::VertexLayout{
            .Stride = 3 + 2 + 3 + 4,
            .OffsetTexCoord = 3,
            .OffsetNormal = 5,
            .OffsetTangent = 8,
        };

        tangen::GenerateTangents(res, layout);
    }
            
    //Fill in the layout:
    if (withTangents)
        res.Layout.VertexLayout = {Vec3, Vec2, Vec3, Vec4};
    else
        res.Layout.VertexLayout = {Vec3, Vec2, Vec3};

    res.Layout.IndexType = VK_INDEX_TYPE_UINT32;

    return res;
}

GeometryData primitive::TexturedCube()
{
    return TexturedCubeImpl(false);
}

GeometryData primitive::TexturedCubeWithTangent()
{
    return TexturedCubeImpl(true);
}

// clang-format on