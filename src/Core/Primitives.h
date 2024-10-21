#pragma once

#include "GeometryProvider.h"
#include "Vertex.h"

#include <cstdint>

struct HelloTriangleVertexProvider : public VertexProvider<ColoredVertex> {
    std::vector<ColoredVertex> GetVertices() override;
};

struct HelloTriangleIndexProvider : public IndexProvider<uint16_t> {
    std::vector<uint16_t> GetIndices() override;
};