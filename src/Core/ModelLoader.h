#pragma once

#include "GeometryProvider.h"

#include <fastgltf/core.hpp>

#include <filesystem>

namespace ModelLoader
{
// To-do: maybe use custom return type instead:
fastgltf::Asset GetGltf(const std::filesystem::path &filepath);

struct ModelConfig {
    std::filesystem::path Filepath;

    bool LoadTexCoord = true;
    bool LoadNormals = true;
    bool LoadTangents = true;
    bool LoadColor = false;
};

GeometryProvider LoadModel(const ModelConfig &config);

GeometryProvider LoadPrimitive(const ModelConfig &config, int64_t meshId,
                               int64_t primitiveId);
} // namespace ModelLoader
