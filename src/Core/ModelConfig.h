#pragma once

#include "VertexLayout.h"

#include <string>

struct ModelConfig {
    // Assumed to be utf8 encoded:
    std::string Filepath;

    // Vertex loading:
    Vertex::Layout VertexLayout = Vertex::PullLayout::Compressed;

    // Material loading:
    bool FetchRoughness = true;
    bool FetchNormal    = true;
};