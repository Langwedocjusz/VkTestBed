#pragma once

#include "GeometryProvider.h"

#include <string>

namespace ModelLoader
{
// To-do: the way this is currently set up
// the gltf is loaded from disk twice.
// Fix this.
GeometryProvider PosTex(const std::string& filepath);
} // namespace ModelLoader
