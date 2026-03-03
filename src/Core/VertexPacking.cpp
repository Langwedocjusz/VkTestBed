#include "VertexPacking.h"
#include "Pch.h"

#include "VertexLayout.h"

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

GeometryData VertexPacking::Encode(GltfPrimitive &prim, Vertex::Layout vLayout)
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

    // Retrieve indices:
    std::memcpy(geo.IndexData.Data, prim.Indices.data(), prim.Indices.size() * sizeof(uint32_t));

    // Repackage vertices based on layout:
    if (auto *layout = std::get_if<Vertex::PushLayout>(&vLayout))
    {
        auto offsets = GetOffsets(*layout);
        const size_t compCount = offsets.ComponentStride * prim.VertexCount;

        auto data = new (geo.VertexData.Data) float[compCount];

        for (size_t vertIdx=0; vertIdx<prim.VertexCount; vertIdx++)
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

    else if (auto* layout = std::get_if<Vertex::PullLayout>(&vLayout))
    {
        switch (*layout)
        {
            case Vertex::PullLayout::Naive: {
                auto data = new (geo.VertexData.Data) Vertex::PullNaive[prim.VertexCount];

                for (size_t vertIdx=0; vertIdx<prim.VertexCount; vertIdx++)
                {
                    data[vertIdx] = Vertex::PullNaive{
                        .Position = prim.Positions[vertIdx],
                        .TexCoordX = prim.TexCoords[vertIdx].x,
                        .Normal = prim.Normals[vertIdx],
                        .TexCoordY = prim.TexCoords[vertIdx].y,
                        .Tangent = prim.Tangents[vertIdx],
                    };
                }

                break;
            }
            case Vertex::PullLayout::Compressed: {
                //TODO: Handle this
                break;
            }
        }
    }    

    return geo;
}