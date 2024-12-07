#pragma once

#include "GeometryProvider.h"

#include <fastgltf/core.hpp>
#include <filesystem>

namespace ModelLoader
{
// To-do, maybe use custom retrun type instead:
fastgltf::Asset GetGltf(const std::filesystem::path &filepath);

struct Config {
    std::filesystem::path Filepath;
    int64_t MeshId;
    int64_t PrimitiveId;
};

GeometryProvider LoadPrimitive(const Config &config);
GeometryProvider LoadModel(const std::filesystem::path &filepath);
} // namespace ModelLoader
