#pragma once

#include "GeometryData.h"

#include <glm/glm.hpp>

namespace primitive
{
GeometryData TexturedCubeWithTangent();
GeometryData TexturedSphereWithTangent(float radius, uint32_t subdivisions);
} // namespace primitive
