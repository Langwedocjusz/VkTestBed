#pragma once

#include "GeometryProvider.h"

namespace primitive
{
GeometryProvider HelloTriangle();
GeometryProvider HelloQuad();

GeometryProvider ColoredCube(glm::vec3 color);
GeometryProvider TexturedQuad();
} // namespace primitive
