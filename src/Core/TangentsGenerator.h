#pragma once

#include "GeometryData.h"

#include <array>

namespace tangen
{
struct VertexLayout {
    uint32_t                Stride;
    std::array<uint32_t, 3> OffsetPos;
    std::array<uint32_t, 2> OffsetTexCoord;
    std::array<uint32_t, 3> OffsetNormal;
    std::array<uint32_t, 4> OffsetTangent;
};

void GenerateTangents(GeometryData &geo, VertexLayout layout);
} // namespace tangen