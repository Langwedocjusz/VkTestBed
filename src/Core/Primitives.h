#pragma once

#include "GeometryData.h"

#include <glm/glm.hpp>

namespace primitive
{
/// Generates a cube with following vertex attributes: {pos, texcoord, normal}
GeometryData TexturedCube();
/// Generates a cube with following vertex attributes: {pos, texcoord, normal, tangent}
GeometryData TexturedCubeWithTangent();
/// Generates a sphere with following vertex attributes: {pos, texcoord, normal, tangent}
GeometryData TexturedSphereWithTangent(float radius, uint32_t subdivisions);
} // namespace primitive
