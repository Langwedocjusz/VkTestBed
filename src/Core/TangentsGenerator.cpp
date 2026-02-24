#include "TangentsGenerator.h"
#include "Pch.h"

#include "mikktspace.h"

struct TgtData {
    GeometryData        *Geo;
    tangen::VertexLayout Layout;
};

static auto GetDataPointers(const SMikkTSpaceContext *ctx)
    -> std::tuple<float *, uint32_t *, tangen::VertexLayout>
{
    auto data = static_cast<TgtData *>(ctx->m_pUserData);
    auto geo  = data->Geo;

    // This is evil, but we know that underlying OpaqueBuffers
    // have appropriate alignment:
    auto vertFloats = (float *)(geo->VertexData.Data);
    // This moreover assumes that indexes are always uint32:
    auto indices = (uint32_t *)(geo->IndexData.Data);

    auto layout = data->Layout;

    return {vertFloats, indices, layout};
}

static int GetBaseComponentIndex(uint32_t *indices, int32_t iVert, int32_t iFace,
                                 uint32_t stride)
{
    auto indexId       = 3 * iFace + iVert;
    auto index         = indices[indexId];
    auto baseCompIndex = index * stride;

    return baseCompIndex;
};

void tangen::GenerateTangents(GeometryData &geo, VertexLayout layout)
{
    TgtData data{
        .Geo    = &geo,
        .Layout = layout,
    };

    SMikkTSpaceInterface interface;

    SMikkTSpaceContext ctx;
    ctx.m_pInterface = &interface;
    ctx.m_pUserData  = &data;

    interface.m_getNumFaces = [](const SMikkTSpaceContext *ctx) {
        auto data = static_cast<TgtData *>(ctx->m_pUserData);
        auto geo  = data->Geo;

        return static_cast<int>(geo->IndexData.Count / 3);
    };

    interface.m_getNumVerticesOfFace = [](const SMikkTSpaceContext *ctx, const int face) {
        (void)ctx;
        (void)face;

        return 3;
    };

    interface.m_getPosition = [](const SMikkTSpaceContext *pContext, float fvPosOut[],
                                 const int iFace, const int iVert) {
        auto [vertFloats, indices, layout] = GetDataPointers(pContext);

        auto baseIdx = GetBaseComponentIndex(indices, iVert, iFace, layout.Stride);

        fvPosOut[0] = vertFloats[baseIdx + layout.OffsetPos[0]];
        fvPosOut[1] = vertFloats[baseIdx + layout.OffsetPos[1]];
        fvPosOut[2] = vertFloats[baseIdx + layout.OffsetPos[2]];
    };

    interface.m_getNormal = [](const SMikkTSpaceContext *pContext, float fvNormOut[],
                               const int iFace, const int iVert) {
        auto [vertFloats, indices, layout] = GetDataPointers(pContext);

        auto baseIdx = GetBaseComponentIndex(indices, iVert, iFace, layout.Stride);

        fvNormOut[0] = vertFloats[baseIdx + layout.OffsetNormal[0]];
        fvNormOut[1] = vertFloats[baseIdx + layout.OffsetNormal[1]];
        fvNormOut[2] = vertFloats[baseIdx + layout.OffsetNormal[2]];
    };

    interface.m_getTexCoord = [](const SMikkTSpaceContext *pContext, float fvTexcOut[],
                                 const int iFace, const int iVert) {
        auto [vertFloats, indices, layout] = GetDataPointers(pContext);

        auto baseIdx = GetBaseComponentIndex(indices, iVert, iFace, layout.Stride);

        fvTexcOut[0] = vertFloats[baseIdx + layout.OffsetTexCoord[0]];
        fvTexcOut[1] = vertFloats[baseIdx + layout.OffsetTexCoord[1]];
    };

    interface.m_setTSpaceBasic = [](const SMikkTSpaceContext *pContext,
                                    const float fvTangent[], const float fSign,
                                    const int iFace, const int iVert) {
        auto [vertFloats, indices, layout] = GetDataPointers(pContext);

        auto baseIdx = GetBaseComponentIndex(indices, iVert, iFace, layout.Stride);

        vertFloats[baseIdx + layout.OffsetTangent[0]] = fvTangent[0];
        vertFloats[baseIdx + layout.OffsetTangent[1]] = fvTangent[1];
        vertFloats[baseIdx + layout.OffsetTangent[2]] = fvTangent[2];
        vertFloats[baseIdx + layout.OffsetTangent[3]] = fSign;
    };

    interface.m_setTSpace =
        [](const SMikkTSpaceContext *pContext, const float fvTangent[],
           const float fvBiTangent[], const float fMagS, const float fMagT,
           const tbool bIsOrientationPreserving, const int iFace, const int iVert) {
            (void)fvBiTangent;
            (void)fMagS;
            (void)fMagT;

            auto [vertFloats, indices, layout] = GetDataPointers(pContext);

            auto baseIdx = GetBaseComponentIndex(indices, iVert, iFace, layout.Stride);

            vertFloats[baseIdx + layout.OffsetTangent[0]] = fvTangent[0];
            vertFloats[baseIdx + layout.OffsetTangent[1]] = fvTangent[1];
            vertFloats[baseIdx + layout.OffsetTangent[2]] = fvTangent[2];
            vertFloats[baseIdx + layout.OffsetTangent[3]] =
                bIsOrientationPreserving ? 1.0f : (-1.0f);
        };

    genTangSpaceDefault(&ctx);
}