#pragma once

#include <glm/glm.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>

#include "GeometryProvider.h"

struct InstanceData {
    glm::vec3 Translation = {0.0f, 0.0f, 0.0f};
    glm::vec3 Rotation = {0.0f, 0.0f, 0.0f};
    glm::vec3 Scale = {1.0f, 1.0f, 1.0f};

    glm::mat4 GetTransform()
    {
        return glm::translate(glm::mat4(1.0f), Translation)
        * glm::eulerAngleXYZ(Rotation.x, Rotation.y, Rotation.z)
        * glm::scale(glm::mat4(1.0f), Scale);
    }
};

struct SceneObject {
    GeometryProvider Provider;
    std::vector<InstanceData> Instances;
    bool UpdateInstances = true;
};

struct Scene {
    std::vector<SceneObject> Objects;
    bool UpdateRequested = true;
    bool GlobalUpdate = true;

    void ClearUpdateFlags()
    {
        UpdateRequested = false;
        GlobalUpdate = false;

        for (auto &obj : Objects)
            obj.UpdateInstances = false;
    }
};