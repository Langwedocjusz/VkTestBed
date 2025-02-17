#pragma once

#include "GeometryData.h"

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>

#include <filesystem>

namespace ModelLoader
{
struct Config {
    std::filesystem::path Filepath;

    bool LoadTexCoord = true;
    bool LoadNormals = true;
    bool LoadTangents = true;
    bool LoadColor = false;

    [[nodiscard]] GeometryLayout GeoLayout() const;
};

// To-do: maybe use custom return type instead:
fastgltf::Asset GetGltf(const std::filesystem::path &filepath);
fastgltf::Asset GetGltfWithBuffers(const std::filesystem::path &filepath);

GeometryData LoadPrimitive(fastgltf::Asset &gltf, const Config &config,
                           fastgltf::Primitive &primitive);
} // namespace ModelLoader
