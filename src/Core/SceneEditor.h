#pragma once

#include "AssetManager.h"
#include "Scene.h"

#include <glm/glm.hpp>

#include <filesystem>
#include <ranges>
#include <variant>

class SceneGraphNode {
  public:
    using ChildrenArray = std::vector<std::unique_ptr<SceneGraphNode>>;

  public:
    SceneGraphNode();
    SceneGraphNode(SceneKey key);

    bool IsLeaf();

    SceneKey GetObjectKey();
    ChildrenArray &GetChildren();

    /// Emplaces a non-leaf node as child
    SceneGraphNode &EmplaceChild();
    /// Emplaces a leaf node as child
    SceneGraphNode &EmplaceChild(SceneKey key);

    glm::mat4 GetTransform();
    void UpdateTransforms(Scene &scene, glm::mat4 current = 1.0f);

  public:
    SceneGraphNode *Parent = nullptr;

    glm::vec3 Translation = {0.0f, 0.0f, 0.0f};
    glm::vec3 Rotation = {0.0f, 0.0f, 0.0f};
    glm::vec3 Scale = {1.0f, 1.0f, 1.0f};

    std::string Name;

  private:
    std::variant<ChildrenArray, SceneKey> Payload;
};

class SceneEditor {
  public:
    SceneEditor(Scene &scene);

    void OnUpdate();

    const SceneMesh &GetMesh(SceneKey key);
    const SceneMaterial &GetMaterial(SceneKey key);
    const SceneObject &GetObject(SceneKey key);

    void EraseMesh(SceneKey mesh);

    SceneKey EmplaceObject(std::optional<SceneKey> mesh);
    SceneKey DuplicateObject(SceneKey obj);
    void EraseObject(SceneKey key);

    void LoadModel(const ModelConfig &config);

    void SetHdri(const std::filesystem::path &path);

    auto Meshes()
    {
        return std::views::all(mScene.Meshes);
    }
    auto Materials()
    {
        return std::views::all(mScene.Materials);
    }

    void UpdateTransforms(SceneGraphNode *rootNode);

    Scene::Environment& GetEnv();
    void RequestUpdate(Scene::UpdateFlag flag);

  public:
    // Root of the scene-graph used by UI to control objects:
    SceneGraphNode GraphRoot;

  private:
    Scene &mScene;

    AssetManager mAssetManager;
};