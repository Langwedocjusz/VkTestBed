#pragma once

#include "GeometryData.h"
#include "ModelConfig.h"

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>

#include <filesystem>

namespace ModelLoader
{
fastgltf::Asset GetGltf(const std::filesystem::path &filepath);
fastgltf::Asset GetGltfWithBuffers(const std::filesystem::path &filepath);

GeometryData LoadPrimitive(fastgltf::Asset &gltf, const ModelConfig &config,
                           fastgltf::Primitive &primitive);
} // namespace ModelLoader
