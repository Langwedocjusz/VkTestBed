#pragma once

#include "GeometryData.h"

#include <glm/glm.hpp>

namespace primitive
{
GeometryData HelloTriangle();
GeometryData HelloQuad();

GeometryData TexturedQuad();

GeometryData ColoredCube();
GeometryData TexturedCube();
GeometryData TexturedCubeWithTangent();
} // namespace primitive
