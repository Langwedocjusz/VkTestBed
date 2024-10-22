#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "GeometryProvider.h"

struct InstanceData {
    glm::vec3 Translation = {0.0f, 0.0f, 0.0f};
    glm::quat Rotation = {1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 Scale = {1.0f, 1.0f, 1.0f};
};

struct SceneObject {
    GeometryProvider Provider;
    std::vector<InstanceData> Instances;
};

struct Scene {
    std::vector<SceneObject> Objects;
};