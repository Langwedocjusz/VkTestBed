#pragma once

#include "AssetManager.h"
#include "SceneGraph.h"

#include <filesystem>
#include <ranges>

class SceneEditor {
  public:
    struct NodeOpData {
        SceneGraphNode *SrcParent;
        int64_t ChildId;
        SceneGraphNode *DstParent;

        SceneGraphNode &GetSourceNode();
        auto GetSourceNodeIterator();
    };

    struct Prefab {
        SceneGraphNode Root;
        bool IsReady = false;
    };

  public:
    SceneEditor(Scene &scene);

    void OnUpdate();

    // Functions to manipulate the underlying scene:
    SceneObject &GetObject(SceneKey key);
    SceneMesh &GetMesh(SceneKey key);
    SceneMaterial &GetMaterial(SceneKey key);
    ImageData &GetImage(SceneKey key);
    Scene::Environment &GetEnv();

    void EraseMesh(SceneKey mesh);

    void LoadModel(const ModelConfig &config);
    void SetHdri(const std::filesystem::path &path);
    void RequestFullReload();
    void RequestUpdate(Scene::UpdateFlag flag);

    // Functions to manipulate the scene-graph:
    void ScheduleNodeMove(NodeOpData data);
    void ScheduleNodeCopy(NodeOpData data);
    void ScheduleNodeDeletion(NodeOpData data);

    void UpdateTransforms(SceneGraphNode *rootNode);

    std::pair<SceneKey, Prefab &> EmplacePrefab(
        std::optional<SceneKey> meshKey = std::nullopt);
    void InstancePrefab(SceneKey prefabId);

    // Iterators to underlying data:

    auto Prefabs()
    {
        return std::views::all(mPrefabs);
    }

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

  public:
    // Root of the scene-graph used by UI to control objects:
    SceneGraphNode GraphRoot;

  private:
    void HandleNodeOp();
    void HandleNodeMove();
    void HandleNodeCopy();
    void HandleNodeDelete();

    SceneKey DuplicateObject(SceneKey obj);
    SceneKey EmplaceObject(std::optional<SceneKey> mesh);

    void CopyNodeTree(SceneGraphNode &source, SceneGraphNode &target);
    void InstancePrefabImpl(SceneGraphNode &source, SceneGraphNode &target);

  private:
    Scene &mScene;
    AssetManager mAssetManager;

    // Trees representing mesh hierarchies of imported gltf scenes.
    // They are grafted onto the main scene-graph when instancing the gltf.
    std::map<SceneKey, Prefab> mPrefabs;

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