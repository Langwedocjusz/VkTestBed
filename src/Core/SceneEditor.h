#pragma once

#include "AssetManager.h"
#include "Scene.h"

#include <glm/glm.hpp>

#include <filesystem>
#include <memory>
#include <ranges>
#include <variant>

class SceneEditor;

class SceneGraphNode {
  public:
    using ChildrenArray = std::vector<std::unique_ptr<SceneGraphNode>>;

  public:
    SceneGraphNode(SceneEditor &editor);
    SceneGraphNode(SceneEditor &editor, SceneKey key);

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
    SceneEditor &mEditor;
    std::variant<ChildrenArray, SceneKey> mPayload;
};

class SceneEditor {
  public:
    SceneEditor(Scene &scene);

    void OnUpdate();

    // Functions to manipulate the underlying scene:
    SceneMesh &GetMesh(SceneKey key);
    SceneMaterial &GetMaterial(SceneKey key);
    ImageData &GetImage(SceneKey key);
    SceneObject &GetObject(SceneKey key);
    Scene::Environment &GetEnv();

    void EraseMesh(SceneKey mesh);

    SceneKey EmplaceObject(std::optional<SceneKey> mesh);
    SceneKey DuplicateObject(SceneKey obj);
    void EraseObject(SceneKey key);

    auto Meshes()
    {
        return std::views::all(mScene.Meshes);
    }
    auto Materials()
    {
        return std::views::all(mScene.Materials);
    }
    auto Images()
    {
        return std::views::all(mScene.Images);
    }

    void LoadModel(const ModelConfig &config);
    void SetHdri(const std::filesystem::path &path);
    void RequestFullReload();
    void RequestUpdate(Scene::UpdateFlag flag);

    // Functions to manipulate the scene-graph:
    struct NodeOpData {
        SceneGraphNode *SrcParent;
        int64_t ChildId;
        SceneGraphNode *DstParent;

        SceneGraphNode &GetSourceNode();
        auto GetSourceNodeIterator();
    };

    void ScheduleNodeMove(NodeOpData data);
    void ScheduleNodeCopy(NodeOpData data);
    void ScheduleNodeDeletion(NodeOpData data);

    void UpdateTransforms(SceneGraphNode *rootNode);

    std::pair<SceneKey, SceneGraphNode &> EmplacePrefab(
        std::optional<SceneKey> meshKey = std::nullopt);
    void InstancePrefab(SceneKey prefabId);

    auto Prefabs()
    {
        return std::views::all(mPrefabs);
    }

  public:
    // Root of the scene-graph used by UI to control objects:
    SceneGraphNode GraphRoot;

  private:
    void HandleNodeOp();
    void HandleNodeMove();
    void HandleNodeCopy();
    void HandleNodeDelete();

  private:
    Scene &mScene;
    AssetManager mAssetManager;

    // Trees representing mesh hierarchies of imported gltf scenes.
    // They are grafted onto the main scene-graph when instancing the gltf.
    std::map<SceneKey, SceneGraphNode> mPrefabs;

    enum class NodeOp
    {
        None,
        Move,
        Delete,
        Copy
    } mNodeOpType;

    NodeOpData mNodeOpData;

    SceneKeyGenerator mPrefabKeyGenerator;
};