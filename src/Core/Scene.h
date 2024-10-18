#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "GeometryProvider.h"

struct InstanceData {
    glm::vec3 Translation;
    glm::quat Rotation;
    glm::vec3 Scale;
};

struct Scene {
    std::vector<GeometryProvider> Providers;
    std::vector<std::vector<InstanceData>> Instances;
};