#pragma once

#include "GeometryData.h"

#include <glm/glm.hpp>

namespace primitive
{
//TODO: It should be possible to select vertex layout of the primitives.
GeometryData TexturedCubeWithTangent();
GeometryData TexturedSphereWithTangent(float radius, uint32_t subdivisions);
} // namespace primitive
