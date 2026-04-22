#include "Primitives.h"
#include "Pch.h"

#include "GeometryData.h"
#include "TangentsGenerator.h"

#include "VertexLayout.h"
#include "VertexPacking.h"
#include "volk.h"

#include <cmath>
#include <cstdint>
#include <numbers>

GeometryData primitive::TexturedCubeWithTangent()
{
    const uint32_t vertCount = 24;
    const uint32_t idxCount  = 36;

    PrimitiveData primData{};
    primData.VertexCount = vertCount;
    primData.IndexCount  = idxCount;
    primData.BBox        = AABB{
        .Center = glm::vec3(0.0f),
        .Extent = glm::vec3(0.5f),
    };

    // Convention is Top Bottom Front Back Right Left

    // clang-format off
    primData.Positions = {
        {-0.5f, 0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, { 0.5f, 0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f},
        {-0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f,-0.5f}, {-0.5f,-0.5f,-0.5f},
        {-0.5f, 0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, { 0.5f,-0.5f, 0.5f}, {-0.5f,-0.5f, 0.5f},
        {-0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f}, {-0.5f,-0.5f,-0.5f},
        { 0.5f,-0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, { 0.5f, 0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f},
        {-0.5f,-0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f,-0.5f}, {-0.5f,-0.5f,-0.5f}
    };

    primData.TexCoords = {
        {0.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f},
        {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f},
        {1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f},
        {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f},
        {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f},
        {0.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}
    };

    primData.Normals = {
        {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
        {0.0f,-1.0f, 0.0f}, {0.0f,-1.0f, 0.0f}, {0.0f,-1.0f, 0.0f}, {0.0f,-1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f,-1.0f}, {0.0f, 0.0f,-1.0f}, {0.0f, 0.0f,-1.0f}, {0.0f, 0.0f,-1.0f},
        {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
        {-1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}
    };

    primData.Indices = {
        0,1,2, 2,3,0, 
        4,6,5, 6,4,7,
        8,10,9, 10,8,11,
        12,13,14, 14,15,12,
        16,18,17, 18,16,19,
        20,21,22, 22,23,20
    };

    // clang-format on

    tangen::GenerateTangents(primData);

    Vertex::Layout layout = Vertex::PullLayout::Naive;
    // Vertex::Layout layout = Vertex::PullLayout::Compressed;
    GeometryData res = VertexPacking::Encode(primData, layout);

    return res;
}

GeometryData primitive::TexturedSphereWithTangent(float radius, uint32_t subdivisions)
{
    // Based on this post:
    // https://gamedev.stackexchange.com/questions/150191/opengl-calculate-uv-sphere-vertices

    const uint32_t numLatitudeLines  = subdivisions;
    const uint32_t numLongitudeLines = subdivisions;

    // One vertex at every latitude-longitude intersection,
    // plus one for the north pole and one for the south.
    // One meridian serves as a UV seam, so we double the vertices there.
    const uint32_t numVertices = numLatitudeLines * (numLongitudeLines + 1) + 2;

    const uint32_t numTriangles = numLatitudeLines * numLongitudeLines * 2;
    const uint32_t numIndices   = 3 * numTriangles;

    PrimitiveData primData{};
    primData.VertexCount = numVertices;
    primData.IndexCount  = numIndices;
    primData.BBox        = AABB{
        .Center = glm::vec3(0.0f),
        .Extent = glm::vec3(radius),
    };

    // Generate base vertex data:
    primData.Positions.resize(numVertices);
    primData.TexCoords.resize(numVertices);
    primData.Normals.resize(numVertices);

    // North pole.
    primData.Positions[0] = glm::vec3(0.0f, radius, 0.0f);
    primData.TexCoords[0] = glm::vec2(0.0f, 1.0f);
    primData.Normals[0]   = glm::vec3(0.0f, 1.0f, 0.0f);

    // South pole.
    primData.Positions[numVertices - 1] = glm::vec3(0.0f, -radius, 0.0f);
    primData.TexCoords[numVertices - 1] = glm::vec2(0.0f, 0.0f);
    primData.Normals[numVertices - 1]   = glm::vec3(0.0f, -1.0f, 0.0f);

    // +1.0f because there's a gap between the poles and the first parallel.
    const float latitudeSpacing  = 1.0f / (static_cast<float>(numLatitudeLines) + 1.0f);
    const float longitudeSpacing = 1.0f / static_cast<float>(numLongitudeLines);

    // Start writing new vertices at position 1
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
            float phi   = (texCoords.y - 0.5f) * std::numbers::pi_v<float>;

            // Usual formula for a vector in spherical coordinates.
            // You can exchange x & z to wind the opposite way around the sphere.
            const glm::vec3 pos{
                radius * std::cos(phi) * std::cos(theta),
                radius * std::sin(phi),
                radius * std::cos(phi) * std::sin(theta),
            };

            primData.Positions[vertId] = pos;
            primData.TexCoords[vertId] = texCoords;
            primData.Normals[vertId]   = glm::normalize(pos);

            // Proceed to the next vertex.
            vertId++;
        }
    }

    // Generate index data:
    primData.Indices.resize(numIndices);

    size_t indexIdx = 0;

    // North pole cap:
    for (uint32_t i = 0; i < numLongitudeLines; i++)
    {
        primData.Indices[indexIdx++] = 0;
        primData.Indices[indexIdx++] = i + 2;
        primData.Indices[indexIdx++] = i + 1;
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
            primData.Indices[indexIdx++] = firstCorner;
            primData.Indices[indexIdx++] = firstCorner + rowLength + 1;
            primData.Indices[indexIdx++] = firstCorner + rowLength;

            // Second triangle of quad: Top-Left, Bottom-Right, Top-Right
            primData.Indices[indexIdx++] = firstCorner;
            primData.Indices[indexIdx++] = firstCorner + 1;
            primData.Indices[indexIdx++] = firstCorner + rowLength + 1;
        }
    }

    // South pole cap:
    const uint32_t pole      = numVertices - 1;
    uint32_t       bottomRow = (numLatitudeLines - 1) * rowLength + 1;

    for (uint32_t i = 0; i < numLongitudeLines; i++)
    {
        primData.Indices[indexIdx++] = pole;
        primData.Indices[indexIdx++] = bottomRow + i;
        primData.Indices[indexIdx++] = bottomRow + i + 1;
    }

    // Generate tangent vectors:
    tangen::GenerateTangents(primData);

    // Pack to the desired vertex format:
    Vertex::Layout layout = Vertex::PullLayout::Naive;
    // Vertex::Layout layout = Vertex::PullLayout::Compressed;
    GeometryData res = VertexPacking::Encode(primData, layout);

    return res;
}
