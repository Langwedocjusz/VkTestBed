#pragma once

#include "GeometryProvider.h"

namespace primitive
{
GeometryProvider HelloTriangle();
GeometryProvider HelloQuad();

GeometryProvider ColoredCube(glm::vec3 color);

GeometryProvider TexturedQuad();
GeometryProvider TexturedCube();
} // namespace primitive
