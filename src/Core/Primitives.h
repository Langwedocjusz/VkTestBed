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

GeometryData TexturedSphereWithTangent(float radius, uint32_t subdivisions);
} // namespace primitive
