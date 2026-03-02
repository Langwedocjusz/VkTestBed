#include "TangentsGenerator.h"
#include "Pch.h"

#include "Vassert.h"

#include "mikktspace.h"

struct TgtData {
    GltfPrimitive *Prim;
};

static GltfPrimitive *GetDataPointer(const SMikkTSpaceContext *ctx)
{
    auto data = static_cast<TgtData *>(ctx->m_pUserData);
    return data->Prim;
}

static int GetVertexId(std::vector<uint32_t> &indices, int32_t iVert, int32_t iFace)
{
    auto indexId = 3 * iFace + iVert;
    auto index   = static_cast<int>(indices[indexId]);

    return index;
};

void tangen::GenerateTangents(GltfPrimitive &prim)
{
    // Make sure primitive is valid:
    vassert(prim.VertexCount > 0, "Vertex count is zero!");
    vassert(prim.IndexCount > 0, "Index count is zero!");
    
    vassert(prim.Positions.size() == prim.VertexCount, "Missing positons!");
    vassert(prim.TexCoords.size() == prim.VertexCount, "Missing texcoords!");
    vassert(prim.Normals.size() == prim.VertexCount, "Missing normals!");

    vassert(prim.Tangents.empty(), "Tangents already present!");

    // Allocate space for tangents:
    prim.Tangents.resize(prim.VertexCount);

    // Run MikktSpace algorithm:
    SMikkTSpaceContext ctx;

    SMikkTSpaceInterface interface;
    ctx.m_pInterface = &interface;

    TgtData data{.Prim = &prim};
    ctx.m_pUserData = &data;

    interface.m_getNumVerticesOfFace = [](const SMikkTSpaceContext *ctx, const int face) {
        (void)ctx;
        (void)face;

        return 3;
    };

    interface.m_getNumFaces = [](const SMikkTSpaceContext *ctx) {
        auto prim = GetDataPointer(ctx);
        return static_cast<int>(prim->IndexCount / 3);
    };

    interface.m_getPosition = [](const SMikkTSpaceContext *pContext, float fvPosOut[],
                                 const int iFace, const int iVert) {
        auto prim = GetDataPointer(pContext);
        auto index = GetVertexId(prim->Indices, iVert, iFace);

        auto pos = prim->Positions[index];

        fvPosOut[0] = pos.x;
        fvPosOut[1] = pos.y;
        fvPosOut[2] = pos.z;
    };

    interface.m_getTexCoord = [](const SMikkTSpaceContext *pContext, float fvTexcOut[],
                                 const int iFace, const int iVert) {
        auto prim = GetDataPointer(pContext);
        auto index = GetVertexId(prim->Indices, iVert, iFace);

        auto texcoord = prim->TexCoords[index];

        fvTexcOut[0] = texcoord.x;
        fvTexcOut[1] = texcoord.y;
    };

    interface.m_getNormal = [](const SMikkTSpaceContext *pContext, float fvNormOut[],
                               const int iFace, const int iVert) {
        auto prim = GetDataPointer(pContext);
        auto index = GetVertexId(prim->Indices, iVert, iFace);

        auto normal = prim->Normals[index];

        fvNormOut[0] = normal.x;
        fvNormOut[1] = normal.y;
        fvNormOut[2] = normal.z;
    };

    interface.m_setTSpaceBasic = [](const SMikkTSpaceContext *pContext,
                                    const float fvTangent[], const float fSign,
                                    const int iFace, const int iVert) {
        auto prim = GetDataPointer(pContext);
        auto index = GetVertexId(prim->Indices, iVert, iFace);

        glm::vec4 tangent{fvTangent[0], fvTangent[1], fvTangent[2], fSign};

        prim->Tangents[index] = tangent;
    };

    interface.m_setTSpace =
        [](const SMikkTSpaceContext *pContext, const float fvTangent[],
           const float fvBiTangent[], const float fMagS, const float fMagT,
           const tbool bIsOrientationPreserving, const int iFace, const int iVert) {
            (void)fvBiTangent;
            (void)fMagS;
            (void)fMagT;

            auto prim = GetDataPointer(pContext);
            auto index = GetVertexId(prim->Indices, iVert, iFace);

            auto sign = bIsOrientationPreserving ? 1.0f : (-1.0f);

            glm::vec4 tangent{fvTangent[0], fvTangent[1], fvTangent[2], sign};

            prim->Tangents[index] = tangent;                
        };

    genTangSpaceDefault(&ctx);
}