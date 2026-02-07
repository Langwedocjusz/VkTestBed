#include "TangentsGenerator.h"
#include "Pch.h"

#include "mikktspace.h"

void tangen::GenerateTangents(GeometryData &geo, VertexLayout layout)
{
    struct TgtData {
        GeometryData *Geo;
        VertexLayout  Layout;
    };

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
        auto data   = static_cast<TgtData *>(pContext->m_pUserData);
        auto geo    = data->Geo;
        auto layout = data->Layout;

        // This is evil, but we know that underlying OpaqueBuffers
        // have appropriate alignment
        auto vertFloats = (float *)(geo->VertexData.Data);
        auto indices    = (uint32_t *)(geo->IndexData.Data);

        auto indexId    = 3 * iFace + iVert;
        auto index      = indices[indexId];
        auto floatIndex = index * layout.Stride;

        fvPosOut[0] = vertFloats[floatIndex + 0];
        fvPosOut[1] = vertFloats[floatIndex + 1];
        fvPosOut[2] = vertFloats[floatIndex + 2];
    };

    interface.m_getNormal = [](const SMikkTSpaceContext *pContext, float fvNormOut[],
                               const int iFace, const int iVert) {
        auto data   = static_cast<TgtData *>(pContext->m_pUserData);
        auto geo    = data->Geo;
        auto layout = data->Layout;

        // This is evil, but we know that underlying OpaqueBuffers
        // have appropriate alignment
        auto vertFloats = (float *)(geo->VertexData.Data);
        auto indices    = (uint32_t *)(geo->IndexData.Data);

        auto indexId    = 3 * iFace + iVert;
        auto index      = indices[indexId];
        auto floatIndex = index * layout.Stride + layout.OffsetNormal;

        fvNormOut[0] = vertFloats[floatIndex + 0];
        fvNormOut[1] = vertFloats[floatIndex + 1];
        fvNormOut[2] = vertFloats[floatIndex + 2];
    };

    interface.m_getTexCoord = [](const SMikkTSpaceContext *pContext, float fvTexcOut[],
                                 const int iFace, const int iVert) {
        auto data   = static_cast<TgtData *>(pContext->m_pUserData);
        auto geo    = data->Geo;
        auto layout = data->Layout;

        // This is evil, but we know that underlying OpaqueBuffers
        // have appropriate alignment
        auto vertFloats = (float *)(geo->VertexData.Data);
        auto indices    = (uint32_t *)(geo->IndexData.Data);

        auto indexId    = 3 * iFace + iVert;
        auto index      = indices[indexId];
        auto floatIndex = index * layout.Stride + layout.OffsetTexCoord;

        fvTexcOut[0] = vertFloats[floatIndex + 0];
        fvTexcOut[1] = vertFloats[floatIndex + 1];
    };

    interface.m_setTSpaceBasic = [](const SMikkTSpaceContext *pContext,
                                    const float fvTangent[], const float fSign,
                                    const int iFace, const int iVert) {
        auto data   = static_cast<TgtData *>(pContext->m_pUserData);
        auto geo    = data->Geo;
        auto layout = data->Layout;

        // This is evil, but we know that underlying OpaqueBuffers
        // have appropriate alignment
        auto vertFloats = (float *)(geo->VertexData.Data);
        auto indices    = (uint32_t *)(geo->IndexData.Data);

        auto indexId    = 3 * iFace + iVert;
        auto index      = indices[indexId];
        auto floatIndex = index * layout.Stride + layout.OffsetTangent;

        vertFloats[floatIndex + 0] = fvTangent[0];
        vertFloats[floatIndex + 1] = fvTangent[1];
        vertFloats[floatIndex + 2] = fvTangent[2];
        vertFloats[floatIndex + 3] = fSign;
    };

    interface.m_setTSpace =
        [](const SMikkTSpaceContext *pContext, const float fvTangent[],
           const float fvBiTangent[], const float fMagS, const float fMagT,
           const tbool bIsOrientationPreserving, const int iFace, const int iVert) {
            (void)fvBiTangent;
            (void)fMagS;
            (void)fMagT;

            auto data   = static_cast<TgtData *>(pContext->m_pUserData);
            auto geo    = data->Geo;
            auto layout = data->Layout;

            // This is evil, but we know that underlying OpaqueBuffers
            // have appropriate alignment
            auto vertFloats = (float *)(geo->VertexData.Data);
            auto indices    = (uint32_t *)(geo->IndexData.Data);

            auto indexId    = 3 * iFace + iVert;
            auto index      = indices[indexId];
            auto floatIndex = index * layout.Stride + layout.OffsetTangent;

            vertFloats[floatIndex + 0] = fvTangent[0];
            vertFloats[floatIndex + 1] = fvTangent[1];
            vertFloats[floatIndex + 2] = fvTangent[2];
            vertFloats[floatIndex + 3] = bIsOrientationPreserving ? 1.0f : (-1.0f);
        };

    genTangSpaceDefault(&ctx);
}