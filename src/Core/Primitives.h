#pragma once

#include "GeometryProvider.h"
#include "Vertex.h"

#include <cstdint>

class HelloTriangleVertexProvvider : public VertexProvider<ColoredVertex> {
    std::vector<ColoredVertex> GetVertices() override;
};

class HelloTriangleIndexProvider : public IndexProvider<uint16_t> {
    std::vector<uint16_t> GetIndices() override;
};