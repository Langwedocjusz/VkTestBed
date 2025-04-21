#include "Primitives.h"
#include "GeometryData.h"
#include "TangentsGenerator.h"

#include <cmath>
#include <cstdint>
#include <numbers>
#include <vulkan/vulkan.h>

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

GeometryData primitive::TexturedSphereWithTangent(float radius, uint32_t subdivisions)
{
    // Based on this post:
    // https://gamedev.stackexchange.com/questions/150191/opengl-calculate-uv-sphere-vertices

    const uint32_t numLatitudeLines = subdivisions;
    const uint32_t numLongitudeLines = subdivisions;

    // One vertex at every latitude-longitude intersection,
    // plus one for the north pole and one for the south.
    // One meridian serves as a UV seam, so we double the vertices there.
    const uint32_t numVertices = numLatitudeLines * (numLongitudeLines + 1) + 2;

    const uint32_t numTriangles = numLatitudeLines * numLongitudeLines * 2;
    const uint32_t numIndices = 3 * numTriangles;

    struct Vertex {
        glm::vec3 Position;
        glm::vec2 TexCoord;
        glm::vec3 Normal;
        glm::vec4 Tangent;
    };

    const auto spec = GeometrySpec::BuildV<Vertex, uint32_t>(numVertices, numIndices);

    GeometryData res(spec);

    auto vertices = new (res.VertexData.Data.get()) Vertex[spec.VertCount];
    auto indices = new (res.IndexData.Data.get()) uint32_t[spec.IdxCount];

    // North pole.
    vertices[0] = Vertex{
        .Position = glm::vec3(0.0f, radius, 0.0f),
        .TexCoord = glm::vec2(0.0f, 1.0f),
        .Normal = glm::vec3(0.0f, 1.0f, 0.0f),
        .Tangent = glm::vec4(0.0f),
    };

    // South pole.
    vertices[numVertices - 1] = Vertex{
        .Position = glm::vec3(0.0f, -radius, 0.0f),
        .TexCoord = glm::vec2(0.0f, 0.0f),
        .Normal = glm::vec3(0.0f, -1.0f, 0.0f),
        .Tangent = glm::vec4(0.0f),
    };

    // +1.0f because there's a gap between the poles and the first parallel.
    const float latitudeSpacing = 1.0f / (static_cast<float>(numLatitudeLines) + 1.0f);
    const float longitudeSpacing = 1.0f / static_cast<float>(numLongitudeLines);

    // start writing new vertices at position 1
    size_t vertId = 1;

    for (uint32_t latitude = 0; latitude < numLatitudeLines; latitude++)
    {
        for (uint32_t longitude = 0; longitude <= numLongitudeLines; longitude++)
        {
            // Scale coordinates into the 0...1 texture coordinate range,
            // with north at the top (y = 1).
            const glm::vec2 texCoords{
                static_cast<float>(longitude) * longitudeSpacing,
                1.0f - static_cast<float>(latitude + 1) * latitudeSpacing,
            };

            // Convert to spherical coordinates:
            // theta is a longitude angle (around the equator) in radians.
            // phi is a latitude angle (north or south of the equator).
            float theta = texCoords.x * 2.0f * std::numbers::pi_v<float>;
            float phi = (texCoords.y - 0.5f) * std::numbers::pi_v<float>;

            // Usual formula for a vector in spherical coordinates.
            // You can exchange x & z to wind the opposite way around the sphere.
            const glm::vec3 pos{
                radius * std::cos(phi) * std::cos(theta),
                radius * std::sin(phi),
                radius * std::cos(phi) * std::sin(theta),
            };

            vertices[vertId] = Vertex{
                .Position = pos,
                .TexCoord = texCoords,
                .Normal = glm::normalize(pos),
                .Tangent = glm::vec4(0.0f),
            };

            // Proceed to the next vertex.
            vertId++;
        }
    }

    // Generate indices:
    size_t indexIdx = 0;

    // North pole cap:
    for (uint32_t i = 0; i < numLongitudeLines; i++)
    {
        indices[indexIdx++] = 0;
        indices[indexIdx++] = i + 2;
        indices[indexIdx++] = i + 1;
    }

    // Middle:
    //  Each row has one more unique vertex than there are lines of longitude,
    //  since we double a vertex at the texture seam.
    const uint32_t rowLength = numLongitudeLines + 1;

    for (uint32_t latitude = 0; latitude < numLatitudeLines - 1; latitude++)
    {
        // Plus one for the pole.
        uint32_t rowStart = latitude * rowLength + 1;

        for (uint32_t longitude = 0; longitude < numLongitudeLines; longitude++)
        {
            uint32_t firstCorner = rowStart + longitude;

            // First triangle of quad: Top-Left, Bottom-Left, Bottom-Right
            indices[indexIdx++] = firstCorner;
            indices[indexIdx++] = firstCorner + rowLength + 1;
            indices[indexIdx++] = firstCorner + rowLength;

            // Second triangle of quad: Top-Left, Bottom-Right, Top-Right
            indices[indexIdx++] = firstCorner;
            indices[indexIdx++] = firstCorner + 1;
            indices[indexIdx++] = firstCorner + rowLength + 1;
        }
    }

    // South pole cap:
    const uint32_t pole = numVertices - 1;
    uint32_t bottomRow = (numLatitudeLines - 1) * rowLength + 1;

    for (uint32_t i = 0; i < numLongitudeLines; i++)
    {
        indices[indexIdx++] = pole;
        indices[indexIdx++] = bottomRow + i;
        indices[indexIdx++] = bottomRow + i + 1;
    }

    // Generate the tangents:
    {
        auto layout = tangen::VertexLayout{
            .Stride = 3 + 2 + 3 + 4,
            .OffsetTexCoord = 3,
            .OffsetNormal = 5,
            .OffsetTangent = 8,
        };

        tangen::GenerateTangents(res, layout);
    }

    // Fill in the layout:
    using enum ::Vertex::AttributeType;

    res.Layout.VertexLayout = {Vec3, Vec2, Vec3, Vec4};
    res.Layout.IndexType = VK_INDEX_TYPE_UINT32;

    return res;
}
