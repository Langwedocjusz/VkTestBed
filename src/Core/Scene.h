#pragma once

#include "Bitflags.h"
#include "GeometryProvider.h"
#include "Material.h"

#include <glm/glm.hpp>

#include <optional>
#include <string>
#include <variant>

struct SceneMesh {
    std::string Name;
    GeometryProvider GeoProvider;
    std::vector<size_t> MaterialIds;
};

struct SceneObject {
    std::optional<size_t> MeshId;

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

    /// Emplaces a non-leaf node as child
    SceneGraphNode &EmplaceChild();
    /// Emplaces a leaf node as child
    SceneGraphNode &EmplaceChild(size_t idx);

    glm::mat4 GetTransform();
    void UpdateTransforms(ObjectArray &objs, glm::mat4 current = 1.0f);

  public:
    SceneGraphNode *Parent = nullptr;

    glm::vec3 Translation = {0.0f, 0.0f, 0.0f};
    glm::vec3 Rotation = {0.0f, 0.0f, 0.0f};
    glm::vec3 Scale = {1.0f, 1.0f, 1.0f};

    std::string Name;

  private:
    std::variant<ChildrenArray, ObjectId> Payload;
};

class Scene {
  public:
    // Emplaces an object in first empty slot:
    size_t EmplaceObject(SceneObject obj);

    // Api to manage update flags, to communicate with renderers:
    bool UpdateRequested();
    void ClearUpdateFlags();

    void RequestFullUpdate();
    void RequestMaterialUpdate();
    void RequestGeometryUpdate();
    void RequestMeshMaterialUpdate();
    void RequestInstanceUpdate();
    void RequestEnvironmentUpdate();

    bool UpdateMaterials();
    bool UpdateGeometry();
    bool UpdateMeshMaterials();
    bool UpdateInstances();
    bool UpdateEnvironment();

  public:
    // Data sources for renderers:
    std::vector<SceneMesh> Meshes;
    std::vector<Material> Materials;

    // Environment texture (hdri) path:
    std::optional<std::filesystem::path> HdriPath;

    // Object instances to be rendered::
    std::vector<std::optional<SceneObject>> Objects;

    // Root of the scene-graph used by UI to control objects:
    SceneGraphNode GraphRoot;

  private:
    enum class Update
    {
        Materials,
        MeshGeo,
        MeshMaterials,
        Instances,
        Environment,
    };

    Bitflags<Update> mUpdateFlags;
};