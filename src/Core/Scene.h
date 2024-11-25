#pragma once

#include <string>
#include <optional>
#include <variant>

#include <glm/glm.hpp>

#include "GeometryProvider.h"
#include "Material.h"

struct SceneObject {
    std::optional<size_t> GeometryId;
    std::optional<size_t> MaterialId;

    glm::mat4 Transform;
};

class SceneGraphNode {
  public:
    using ObjectId = size_t;
    using ChildrenArray = std::vector<std::unique_ptr<SceneGraphNode>>;
    using ObjectArray = std::vector<std::optional<SceneObject>>;

    SceneGraphNode();
    SceneGraphNode(size_t instanceId);

    bool IsLeaf();
    size_t GetIndex();
    ChildrenArray &GetChildren();

    void AddChild();
    void AddChild(size_t idx);

    glm::mat4 GetTransform();
    void UpdateTransforms(ObjectArray &objs, glm::mat4 current = 1.0f);

  public:
    glm::vec3 Translation = {0.0f, 0.0f, 0.0f};
    glm::vec3 Rotation = {0.0f, 0.0f, 0.0f};
    glm::vec3 Scale = {1.0f, 1.0f, 1.0f};

    std::string Name;

  private:
    std::variant<ChildrenArray, ObjectId> Payload;
};

struct Scene {
    // Data sources for renderers:
    std::vector<GeometryProvider> GeoProviders;
    std::vector<Material> Materials;

    // Object instances to be rendered::
    std::vector<std::optional<SceneObject>> Objects;

    size_t EmplaceObject(SceneObject obj);

    // Root of the scene-graph used by UI to control objects:
    SceneGraphNode GraphRoot;

    // Update flags to inform renderers:
    bool UpdateRequested = true;
    bool GlobalUpdate = true;

    void ClearUpdateFlags();
};