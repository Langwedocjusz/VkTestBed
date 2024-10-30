#pragma once

#include "GeometryProvider.h"
#include "Vertex.h"

#include <string>


struct TexturedVertexLoader{
    void LoadGltf(const std::string& filepath);

    //To-do: This is horrible, because all data is copied
    //needlesly - fix this:
    GeometryProvider GetProvider();

    std::vector<Vertex_PXN> Vertices;
    std::vector<uint32_t> Indices;
};

