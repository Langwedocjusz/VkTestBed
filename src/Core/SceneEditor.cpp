#include "SceneEditor.h"
#include "Pch.h"

#include "Primitives.h"
#include "Scene.h"
#include "Vassert.h"

#include <algorithm>
#include <memory>
#include <utility>

// NodeOpData Implementation:
SceneGraphNode &SceneEditor::NodeOpData::GetSourceNode()
{
    auto &children = SrcParent->GetChildren();
    auto &ptr = children[ChildId];
    return *ptr;
}

auto SceneEditor::NodeOpData::GetSourceNodeIterator()
{
    auto &children = SrcParent->GetChildren();
    return children.begin() + static_cast<int64_t>(ChildId);
}

// SceneEditor Implementation:

SceneEditor::SceneEditor(Scene &scene)
    : GraphRoot(&scene), mScene(scene), mAssetManager(scene)
{
    // Emplace test material
    auto [imgKey, img] = scene.EmplaceImage();
    img = ImageData::SinglePixel(Pixel{.R = 255, .G = 255, .B = 255, .A = 255}, false);

    auto [matKey, mat] = scene.EmplaceMaterial();
    mat.Name = "Pure white";
    mat.Albedo = imgKey;

    // Add test cube:
    {
        // Emplace mesh:
        auto [meshKey, mesh] = mScene.EmplaceMesh();

        mesh.Name = "Test Cube";
        auto &prim = mesh.Primitives.emplace_back();
        prim.Data = primitive::TexturedCubeWithTangent();
        prim.Material = matKey;

        // Construct prefab:
        auto [_, prefab] = EmplacePrefab(meshKey);
        prefab.Root.Name = "Test Cube";
        prefab.IsReady = true;
    }

    // Add test sphere:
    {
        // Emplace mesh:
        auto [meshKey, mesh] = mScene.EmplaceMesh();

        mesh.Name = "Test Sphere";
        auto &prim = mesh.Primitives.emplace_back();
        prim.Data = primitive::TexturedSphereWithTangent(0.5f, 24);
        prim.Material = matKey;

        // Construct prefab:
        auto [_, prefab] = EmplacePrefab(meshKey);
        prefab.Root.Name = "Test Sphere";
        prefab.IsReady = true;
    }

    // Request update:
    mScene.RequestUpdateAll();
}

void SceneEditor::OnUpdate()
{
    // Handle node operations if any were scheduled:
    HandleNodeOp();

    mAssetManager.OnUpdate();
}

SceneMesh &SceneEditor::GetMesh(SceneKey key)
{
    vassert(mScene.Meshes.count(key) != 0);

    return mScene.Meshes[key];
}

SceneMaterial &SceneEditor::GetMaterial(SceneKey key)
{
    vassert(mScene.Materials.count(key) != 0);

    return mScene.Materials[key];
}

ImageData &SceneEditor::GetImage(SceneKey key)
{
    vassert(mScene.Images.count(key) != 0);

    return mScene.Images[key];
}

SceneObject &SceneEditor::GetObject(SceneKey key)
{
    vassert(mScene.Objects.count(key) != 0);

    return mScene.Objects[key];
}

Scene::Environment &SceneEditor::GetEnv()
{
    return mScene.Env;
}

void SceneEditor::EraseMesh(SceneKey mesh)
{
    mScene.Meshes.erase(mesh);
    GraphRoot.RemoveChildrenWithMesh(mScene, mesh);

    // To-do: Instead of outright removing this prefab
    // it should probably only remove the nodes associaded
    // with mesh being erased.
    std::erase_if(mPrefabs, [mesh](const auto &item) {
        const auto &[_, prefab] = item;

        return prefab.Root.SubTreeContains(mesh);
    });

    mScene.RequestUpdate(Scene::UpdateFlag::Meshes);
    mScene.RequestUpdate(Scene::UpdateFlag::Objects);
}

void SceneEditor::EraseImage(SceneKey img)
{
    mScene.Images.erase(img);

    auto ResetImageRef = [img](std::optional<SceneKey> &opt) {
        if (opt == img)
            opt = std::nullopt;
    };

    for (auto &[_, mat] : mScene.Materials)
    {
        ResetImageRef(mat.Albedo);
        ResetImageRef(mat.Roughness);
        ResetImageRef(mat.Normal);
    }

    mScene.RequestUpdate(Scene::UpdateFlag::Images);
    mScene.RequestUpdate(Scene::UpdateFlag::Materials);
}

void SceneEditor::ClearCachedHDRI()
{
    mAssetManager.ClearCachedHDRI();
}

SceneKey SceneEditor::EmplaceObject(std::optional<SceneKey> mesh)
{
    if (mesh)
        vassert(mScene.Meshes.count(*mesh) != 0);

    auto [key, obj] = mScene.EmplaceObject();

    obj.Mesh = mesh;

    return key;
}

SceneKey SceneEditor::DuplicateObject(SceneKey obj)
{
    auto &oldObj = GetObject(obj);
    auto [key, _] = mScene.EmplaceObject(oldObj);
    return key;
}

void SceneEditor::UpdateTransforms(SceneGraphNode *rootNode)
{
    vassert(rootNode != nullptr);

    rootNode->UpdateTransforms(mScene);

    mScene.RequestUpdate(Scene::UpdateFlag::Objects);
}

void SceneEditor::LoadModel(const ModelConfig &config)
{
    // Append root of the hierarchy to scene editor prefabs:
    auto [_, prefab] = EmplacePrefab();

    auto &root = prefab.Root;
    root.Name = config.Filepath.stem().string();

    mAssetManager.LoadModel(config, root, prefab.IsReady);
}

void SceneEditor::SetHdri(const std::filesystem::path &path)
{
    mAssetManager.LoadHdri(path);
}

void SceneEditor::RequestFullReload()
{
    mScene.RequestFullReload();
}

void SceneEditor::RequestUpdate(Scene::UpdateFlag flag)
{
    mScene.RequestUpdate(flag);
}

void SceneEditor::ScheduleNodeMove(NodeOpData data)
{
    mNodeOpType = NodeOp::Move;
    mNodeOpData = data;
}

void SceneEditor::ScheduleNodeCopy(NodeOpData data)
{
    mNodeOpType = NodeOp::Copy;
    mNodeOpData = data;
}

void SceneEditor::ScheduleNodeDeletion(NodeOpData data)
{
    mNodeOpType = NodeOp::Delete;
    mNodeOpData = data;
}

void SceneEditor::HandleNodeOp()
{
    switch (mNodeOpType)
    {
    case NodeOp::Move: {
        HandleNodeMove();
        // To-do: this can be optimized to only update
        // affected nodes:
        UpdateTransforms(&GraphRoot);
        break;
    }
    case NodeOp::Delete: {
        HandleNodeDelete();
        mScene.RequestUpdate(Scene::UpdateFlag::Objects);
        break;
    }
    case NodeOp::Copy: {
        HandleNodeCopy();
        mScene.RequestUpdate(Scene::UpdateFlag::Objects);
        break;
    }
    case NodeOp::None: {
        break;
    }
    }

    mNodeOpType = NodeOp::None;
}

void SceneEditor::HandleNodeMove()
{
    // We assume src and dst are different, since
    // move operation wouldn't be scheduled otherwise.
    vassert(mNodeOpData.SrcParent != mNodeOpData.DstParent);

    auto &srcChildren = mNodeOpData.SrcParent->GetChildren();
    auto &dstChildren = mNodeOpData.DstParent->GetChildren();

    // Move to dst, erase from src:
    auto iter = mNodeOpData.GetSourceNodeIterator();

    dstChildren.push_back(std::move(*iter));
    srcChildren.erase(iter);
}

void SceneEditor::CopyNodeTree(SceneGraphNode &source, SceneGraphNode &target)
{
    auto &newNode = [&]() -> SceneGraphNode & {
        if (source.IsLeaf())
        {
            auto key = DuplicateObject(source.GetObjectKey());
            return target.EmplaceChild(key);
        }
        else
            return target.EmplaceChild();
    }();

    newNode.Name = source.Name;
    newNode.Translation = source.Translation;
    newNode.Rotation = source.Rotation;
    newNode.Scale = source.Scale;

    if (!source.IsLeaf())
    {
        for (auto &child : source.GetChildren())
        {
            CopyNodeTree(*child, newNode);
        }
    }
}

void SceneEditor::HandleNodeCopy()
{
    auto &oldNode = mNodeOpData.GetSourceNode();
    CopyNodeTree(oldNode, *mNodeOpData.DstParent);
}

void SceneEditor::HandleNodeDelete()
{
    auto &children = mNodeOpData.SrcParent->GetChildren();
    auto it = mNodeOpData.GetSourceNodeIterator();

    children.erase(it);
}

std::pair<SceneKey, SceneEditor::Prefab &> SceneEditor::EmplacePrefab(
    std::optional<SceneKey> meshKey)
{
    auto key = mPrefabKeyGenerator.Get();

    vassert(mPrefabs.count(key) == 0);

    if (meshKey)
    {
        mPrefabs.emplace(key, SceneEditor::Prefab{
                                  .Root = SceneGraphNode(*meshKey),
                                  .IsReady = false,
                              });
    }
    else
    {
        mPrefabs.emplace(key, SceneEditor::Prefab{
                                  .Root = SceneGraphNode(),
                                  .IsReady = false,
                              });
    }

    return {key, mPrefabs.at(key)};
}

void SceneEditor::InstancePrefabImpl(SceneGraphNode &source, SceneGraphNode &target)
{
    auto &newNode = [&]() -> SceneGraphNode & {
        if (source.IsLeaf())
        {
            auto key = EmplaceObject(source.GetObjectKey());

            return target.EmplaceChild(key);
        }
        else
            return target.EmplaceChild();
    }();

    newNode.Name = source.Name;
    newNode.Translation = source.Translation;
    newNode.Rotation = source.Rotation;
    newNode.Scale = source.Scale;

    if (!source.IsLeaf())
    {
        for (auto &child : source.GetChildren())
        {
            InstancePrefabImpl(*child, newNode);
        }
    }
}

void SceneEditor::InstancePrefab(SceneKey prefabId)
{
    vassert(mPrefabs.count(prefabId) != 0);

    auto &prefab = mPrefabs.at(prefabId);

    InstancePrefabImpl(prefab.Root, GraphRoot);
    UpdateTransforms(&GraphRoot);
}