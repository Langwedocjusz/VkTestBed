#pragma once

#include "GeometryData.h"
#include "ModelConfig.h"

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>

#include <filesystem>
#include <memory>

namespace ModelLoader
{
// To-do: maybe use custom return type instead:
std::unique_ptr<fastgltf::Asset> GetGltf(const std::filesystem::path &filepath);
std::unique_ptr<fastgltf::Asset> GetGltfWithBuffers(
    const std::filesystem::path &filepath);

GeometryData LoadPrimitive(fastgltf::Asset &gltf, const ModelConfig &config,
                           fastgltf::Primitive &primitive);
} // namespace ModelLoader
