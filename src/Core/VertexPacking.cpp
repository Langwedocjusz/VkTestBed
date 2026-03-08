#include "VertexPacking.h"
#include "Pch.h"

#include "Vassert.h"
#include "VertexLayout.h"
#include "glm/vector_relational.hpp"

#include <cstring>

struct PushVertexOffsets {
    uint32_t                  ComponentStride;
    static constexpr uint32_t OffsetPos      = 0;
    static constexpr uint32_t OffsetTexCoord = 3;
    uint32_t                  OffsetNormal;
    uint32_t                  OffsetTangent;
    uint32_t                  OffsetColor;
};

static PushVertexOffsets GetOffsets(const Vertex::PushLayout &layout)
{
    uint32_t stride        = 3;
    uint32_t offsetNormal  = 3;
    uint32_t offsetTangent = 3;
    uint32_t offsetColor   = 3;

    if (layout.HasTexCoord)
    {
        stride += 2;
        offsetNormal += 2;
        offsetTangent += 2;
        offsetColor += 2;
    }

    if (layout.HasNormal)
    {
        stride += 3;
        offsetTangent += 3;
        offsetColor += 3;
    }

    if (layout.HasTangent)
    {
        stride += 4;
        offsetColor += 4;
    }

    if (layout.HasColor)
    {
        stride += 4;
    }

    return PushVertexOffsets{
        .ComponentStride = stride,
        .OffsetNormal    = offsetNormal,
        .OffsetTangent   = offsetTangent,
        .OffsetColor     = offsetColor,
    };
}

static uint16_t QuantizeNormalized(float value)
{
    vassert(0.0f <= value && value <= 1.0f, "Value must be normalied!");

    const float maximum = std::numeric_limits<uint16_t>::max();
    return static_cast<uint16_t>(maximum * value);
}

static std::array<uint16_t, 2> QuantizeVec2(glm::vec2 v)
{
    return {
        QuantizeNormalized(v.x),
        QuantizeNormalized(v.y),
    };
};

static std::array<uint16_t, 3> QuantizeVec3(glm::vec3 v)
{
    return {
        QuantizeNormalized(v.x),
        QuantizeNormalized(v.y),
        QuantizeNormalized(v.z),
    };
};

template <typename T>
static bool IsNormalized(T v)
{
    bool lower = glm::all(glm::greaterThanEqual(v, T(0.0f)));
    bool upper = glm::all(glm::lessThanEqual(v, T(1.0f)));
    return lower && upper;
}

GeometryData VertexPacking::Encode(PrimitiveData &prim, Vertex::Layout vLayout)
{
    // Allocate memory:
    auto       vertSize = Vertex::GetSize(vLayout);
    const auto spec =
        GeometrySpec::BuildS<uint32_t>(vertSize, prim.VertexCount, prim.IndexCount);

    auto geo = GeometryData(spec);

    // Store metadata:
    geo.Layout = GeometryLayout{
        .VertexLayout = vLayout,
        .IndexType    = VK_INDEX_TYPE_UINT32,
    };

    // Store bounding box:
    // If using compressed vertex layout positions will be normalized:
    if (vLayout == Vertex::PullLayout::Compressed)
    {
        geo.BBox = AABB{.Center = glm::vec3(0.0f), .Extent = glm::vec3(1.0f)};
    }   
    else
    {
        geo.BBox = prim.BBox;
    }

    // Retrieve indices:
    std::memcpy(geo.IndexData.Data, prim.Indices.data(),
                prim.Indices.size() * sizeof(uint32_t));

    // Repackage vertices based on layout:
    if (auto *layout = std::get_if<Vertex::PushLayout>(&vLayout))
    {
        auto         offsets   = GetOffsets(*layout);
        const size_t compCount = offsets.ComponentStride * prim.VertexCount;

        auto data = new (geo.VertexData.Data) float[compCount];

        for (size_t vertIdx = 0; vertIdx < prim.VertexCount; vertIdx++)
        {
            auto i = offsets.ComponentStride * vertIdx;

            data[i + offsets.OffsetPos + 0] = prim.Positions[vertIdx].x;
            data[i + offsets.OffsetPos + 1] = prim.Positions[vertIdx].y;
            data[i + offsets.OffsetPos + 2] = prim.Positions[vertIdx].z;

            if (layout->HasTexCoord)
            {
                data[i + offsets.OffsetTexCoord + 0] = prim.TexCoords[vertIdx].x;
                data[i + offsets.OffsetTexCoord + 1] = prim.TexCoords[vertIdx].y;
            }

            if (layout->HasNormal)
            {
                data[i + offsets.OffsetNormal + 0] = prim.Normals[vertIdx].x;
                data[i + offsets.OffsetNormal + 1] = prim.Normals[vertIdx].y;
                data[i + offsets.OffsetNormal + 2] = prim.Normals[vertIdx].z;
            }

            if (layout->HasTangent)
            {
                data[i + offsets.OffsetTangent + 0] = prim.Tangents[vertIdx].x;
                data[i + offsets.OffsetTangent + 1] = prim.Tangents[vertIdx].y;
                data[i + offsets.OffsetTangent + 2] = prim.Tangents[vertIdx].z;
                data[i + offsets.OffsetTangent + 3] = prim.Tangents[vertIdx].w;
            }

            if (layout->HasColor)
            {
                data[i + offsets.OffsetColor + 0] = prim.Colors[vertIdx].x;
                data[i + offsets.OffsetColor + 1] = prim.Colors[vertIdx].y;
                data[i + offsets.OffsetColor + 2] = prim.Colors[vertIdx].z;
                data[i + offsets.OffsetColor + 3] = prim.Colors[vertIdx].w;
            }
        }
    }

    else if (auto *layout = std::get_if<Vertex::PullLayout>(&vLayout))
    {
        switch (*layout)
        {
        case Vertex::PullLayout::Naive: {
            auto data = new (geo.VertexData.Data) Vertex::PullNaive[prim.VertexCount];

            for (size_t vertIdx = 0; vertIdx < prim.VertexCount; vertIdx++)
            {
                data[vertIdx] = Vertex::PullNaive{
                    .Position  = prim.Positions[vertIdx],
                    .TexCoordX = prim.TexCoords[vertIdx].x,
                    .Normal    = prim.Normals[vertIdx],
                    .TexCoordY = prim.TexCoords[vertIdx].y,
                    .Tangent   = prim.Tangents[vertIdx],
                };
            }

            break;
        }
        case Vertex::PullLayout::Compressed: {
            auto data =
                new (geo.VertexData.Data) Vertex::PullCompressed[prim.VertexCount];

            for (size_t vertIdx = 0; vertIdx < prim.VertexCount; vertIdx++)
            {
                glm::vec3 pos      = prim.Positions[vertIdx];
                glm::vec2 texcoord = prim.TexCoords[vertIdx];
                glm::vec3 normal   = prim.Normals[vertIdx];
                glm::vec4 tangent  = prim.Tangents[vertIdx];

                // Normalize position to [0,1]^3:
                pos = pos - prim.BBox.Center;
                // TODO: Think if there is a better way to prevent division by zero:
                pos = pos / (prim.BBox.Extent + 0.001f);
                pos = 0.5f * pos + 0.5f;

                // Normalize texcoords to [0,1]^2:
                texcoord = texcoord - prim.TexBounds.Center;
                texcoord = texcoord / (prim.TexBounds.Extent);
                texcoord = 0.5f * texcoord + 0.5f;

                // Normalize normal to [0,1]^3:
                normal = 0.5f * normal + 0.5f;

                // Normalize tangent to [0,1]^3 x {0,1}:
                auto tan3 = glm::vec3(tangent);
                tan3      = 0.5f * tan3 + 0.5f;

                // Do clamp to catch small numerical inaccuracies:
                pos      = glm::clamp(pos, 0.0f, 1.0f);
                texcoord = glm::clamp(texcoord, 0.0f, 1.0f);
                normal   = glm::clamp(normal, 0.0f, 1.0f);
                tan3     = glm::clamp(tan3, 0.0f, 1.0f);

                std::string message = std::format("Center: {},{} Extent: {},{} Texcoord: {},{}", 
                    prim.TexBounds.Center.x, prim.TexBounds.Center.y, 
                    prim.TexBounds.Extent.x, prim.TexBounds.Extent.y, 
                    texcoord.x, texcoord.y);

                vassert(IsNormalized(pos));
                vassert(IsNormalized(texcoord), message);
                vassert(IsNormalized(normal));
                vassert(IsNormalized(tan3));

                // Quantize to uint16 and store:
                auto qSign    = static_cast<uint16_t>(tangent.w > 0.0f);
                auto qTangent = QuantizeVec3(tan3);

                data[vertIdx] = Vertex::PullCompressed{
                    .Pos      = QuantizeVec3(pos),
                    .TexCoord = QuantizeVec2(texcoord),
                    .Normal   = QuantizeVec3(normal),
                    .Tangent  = {qTangent[0], qTangent[1], qTangent[2], qSign},
                };
            }

            break;
        }
        }
    }

    return geo;
}