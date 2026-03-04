#pragma once

#include "VertexLayout.h"

#include <filesystem>

struct ModelConfig {
    std::filesystem::path Filepath;

    // Vertex loading:
    Vertex::Layout VertexLayout = Vertex::PullLayout::Naive;

    // Material loading:
    bool FetchRoughness = true;
    bool FetchNormal    = true;
};