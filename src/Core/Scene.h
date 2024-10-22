#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "GeometryProvider.h"

struct InstanceData {
    glm::vec3 Translation;
    glm::quat Rotation;
    glm::vec3 Scale;
};

struct SceneObject {
    GeometryProvider Provider;
    std::vector<InstanceData> Instances;
};

struct Scene {
    std::vector<SceneObject> Objects;
};