#pragma once

#include "Scene.h"

#include <glm/glm.hpp>

#include <memory>
#include <variant>

class SceneGraphNode {
  public:
    using ChildrenArray = std::vector<std::unique_ptr<SceneGraphNode>>;

  public:
    SceneGraphNode(Scene &scene);
    SceneGraphNode(Scene &scene, SceneKey key);

    SceneGraphNode(const SceneGraphNode &) = delete;
    SceneGraphNode &operator=(const SceneGraphNode &) = delete;

    SceneGraphNode(SceneGraphNode &&) noexcept;
    SceneGraphNode &operator=(SceneGraphNode &&) = delete;

    ~SceneGraphNode();

    [[nodiscard]] bool IsLeaf() const;
    [[nodiscard]] SceneKey GetObjectKey() const;

    ChildrenArray &GetChildren();
    [[nodiscard]] const ChildrenArray &GetChildrenConst() const;

    /// Emplaces a non-leaf node as child
    SceneGraphNode &EmplaceChild();
    /// Emplaces a leaf node as child
    SceneGraphNode &EmplaceChild(SceneKey key);

    [[nodiscard]] bool SubTreeContains(SceneKey key) const;
    void RemoveChildrenWithMesh(Scene &scene, SceneKey mesh);

    glm::mat4 GetTransform();
    void UpdateTransforms(Scene &scene, glm::mat4 current = 1.0f);

  public:
    SceneGraphNode *Parent = nullptr;

    glm::vec3 Translation = {0.0f, 0.0f, 0.0f};
    glm::vec3 Rotation = {0.0f, 0.0f, 0.0f};
    glm::vec3 Scale = {1.0f, 1.0f, 1.0f};

    std::string Name;

  private:
    Scene &mScene;
    std::variant<ChildrenArray, SceneKey> mPayload;
};