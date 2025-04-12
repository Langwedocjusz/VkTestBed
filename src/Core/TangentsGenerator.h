#pragma once

#include "GeometryData.h"

namespace tangen
{
struct VertexLayout {
    uint32_t Stride;
    uint32_t OffsetTexCoord;
    uint32_t OffsetNormal;
    uint32_t OffsetTangent;
};

void GenerateTangents(GeometryData &geo, VertexLayout layout);
} // namespace tangen