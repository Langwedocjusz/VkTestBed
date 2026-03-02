#include "VertexPacking.h"
#include "Pch.h"

//TODO: This might be useful for packing:
/*static tangen::VertexLayout GetLayout(const ModelConfig &config)
{
    // TODO: PushLayout code:
    // uint32_t stride        = 3;
    // uint32_t offsetTex     = 3;
    // uint32_t offsetNormal  = 3;
    // uint32_t offsetTangent = 3;
    //
    // if (config.LoadTexCoord)
    //{
    //    stride += 2;
    //    offsetNormal += 2;
    //    offsetTangent += 2;
    //}
    //
    // if (config.LoadNormals)
    //{
    //    stride += 3;
    //    offsetTangent += 3;
    //}
    //
    // if (config.LoadTangents)
    //{
    //    stride += 4;
    //}
    //
    //// TODO: support loading colors...
    //
    // return tangen::VertexLayout{
    //    .Stride         = stride,
    //    .OffsetPos      = {0, 1, 2},
    //    .OffsetTexCoord = {offsetTex, offsetTex + 1},
    //    .OffsetNormal   = {offsetNormal, offsetNormal + 1, offsetNormal + 2},
    //    .OffsetTangent  = {offsetTangent, offsetTangent + 1, offsetTangent + 2,
    //                       offsetTangent + 3},
    //};
    (void)config;

    return tangen::VertexLayout{
        .Stride         = 12,
        .OffsetPos      = {0, 1, 2},
        .OffsetTexCoord = {3, 7},
        .OffsetNormal   = {4, 5, 6},
        .OffsetTangent  = {8, 9, 10, 11},
    };
}*/


GeometryData VertexPacking::Encode(GltfPrimitive& prim, Vertex::Layout layout)
{
    auto vertSize = Vertex::GetSize(layout);
    const auto spec = GeometrySpec::BuildS<uint32_t>(vertSize, prim.VertexCount, prim.IndexCount);

    auto geo   = GeometryData(spec);
    
    geo.Layout = GeometryLayout{
        .VertexLayout = layout,
        .IndexType = VK_INDEX_TYPE_UINT32,
    };

    //TODO: Pack data based on vertex layout
    //TODO: Retrieve Index data from primitive

    return geo;
}