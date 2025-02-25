#pragma once

#include <filesystem>

struct ModelConfig {
    std::filesystem::path Filepath;

    // Vertex loading:
    bool LoadTexCoord = true;
    bool LoadNormals = true;
    bool LoadTangents = true;
    bool LoadColor = false;

    // Material loading:
    bool FetchRoughness = true;
    bool FetchNormal = true;
};