#include "SceneEditor.h"
#include "Primitives.h"
#include <algorithm>
#include <memory>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/quaternion.hpp>

#include <libassert/assert.hpp>

#include <iostream>
#include <utility>

// SceneGraphNode Implementation:

SceneGraphNode::SceneGraphNode(SceneEditor &editor) : mEditor(editor)
{
    mPayload = ChildrenArray{};
}

SceneGraphNode::SceneGraphNode(SceneEditor &editor, SceneKey key) : mEditor(editor)
{
    mPayload = key;
}

SceneGraphNode::SceneGraphNode(SceneGraphNode &&other) noexcept
    : Parent(other.Parent), Translation(other.Translation), Rotation(other.Rotation),
      Scale(other.Scale), Name(other.Name), mEditor(other.mEditor),
      mPayload(std::move(other.mPayload))
{
}

SceneGraphNode::~SceneGraphNode()
{
    if (IsLeaf())
    {
        // Remove object, the node pointed to:
        mEditor.EraseObject(GetObjectKey());
    }
}

bool SceneGraphNode::IsLeaf() const
{
    return std::holds_alternative<SceneKey>(mPayload);
}

SceneKey SceneGraphNode::GetObjectKey() const
{
    ASSERT(IsLeaf(), "Only leaf nodes hold object keys!");

    return std::get<SceneKey>(mPayload);
}

SceneGraphNode::ChildrenArray &SceneGraphNode::GetChildren()
{
    ASSERT(!IsLeaf(), "Leaf nodes have no children!");

    return std::get<ChildrenArray>(mPayload);
}

const SceneGraphNode::ChildrenArray &SceneGraphNode::GetChildrenConst() const
{
    ASSERT(!IsLeaf(), "Leaf nodes have no children!");

    return std::get<ChildrenArray>(mPayload);
}

SceneGraphNode &SceneGraphNode::EmplaceChild()
{
    GetChildren().push_back(std::make_unique<SceneGraphNode>(mEditor));

    auto &child = *GetChildren().back();
    child.Parent = this;

    return child;
}

SceneGraphNode &SceneGraphNode::EmplaceChild(SceneKey key)
{
    GetChildren().push_back(std::make_unique<SceneGraphNode>(mEditor, key));

    auto &child = *GetChildren().back();
    child.Parent = this;

    return child;
}

bool SceneGraphNode::SubTreeContains(SceneKey key) const
{
    if (IsLeaf())
        return GetObjectKey() == key;

    else
    {
        for (auto &child : GetChildrenConst())
        {
            if (child->SubTreeContains(key))
                return true;
        }
    }

    return false;
}

void SceneGraphNode::RemoveChildrenWithMesh(Scene &scene, SceneKey mesh)
{
    ASSERT(IsLeaf() == false);

    std::erase_if(GetChildren(), [&scene, mesh](const auto &elem) {
        if (elem->IsLeaf())
        {
            auto &obj = scene.Objects[elem->GetObjectKey()];

            if (obj.Mesh == mesh)
            {
                return true;
            }
        }

        return false;
    });

    for (auto &child : GetChildren())
    {
        if (!child->IsLeaf())
            child->RemoveChildrenWithMesh(scene, mesh);
    }
}

glm::mat4 SceneGraphNode::GetTransform()
{
    return glm::translate(glm::mat4(1.0f), Translation) *
           glm::toMat4(glm::quat(Rotation)) * glm::scale(glm::mat4(1.0f), Scale);
}

void SceneGraphNode::UpdateTransforms(Scene &scene, glm::mat4 current)
{
    if (IsLeaf())
    {
        auto &obj = scene.Objects[GetObjectKey()];

        obj.Transform = current * GetTransform();
    }

    else
    {
        for (auto &child : GetChildren())
        {
            child->UpdateTransforms(scene, current * GetTransform());
        }
    }
}

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
    : GraphRoot(*this), mScene(scene), mAssetManager(scene, *this)
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
        auto [_, root] = EmplacePrefab(meshKey);
        root.Name = "Test Cube";
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
        auto [_, root] = EmplacePrefab(meshKey);
        root.Name = "Test Sphere";
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
    ASSERT(mScene.Meshes.count(key) != 0);

    return mScene.Meshes[key];
}

SceneMaterial &SceneEditor::GetMaterial(SceneKey key)
{
    ASSERT(mScene.Materials.count(key) != 0);

    return mScene.Materials[key];
}

ImageData &SceneEditor::GetImage(SceneKey key)
{
    ASSERT(mScene.Images.count(key) != 0);

    return mScene.Images[key];
}

SceneObject &SceneEditor::GetObject(SceneKey key)
{
    ASSERT(mScene.Objects.count(key) != 0);

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

    std::erase_if(mPrefabs, [mesh](const auto &item) {
        const auto &[key, value] = item;

        return value.SubTreeContains(mesh);
    });

    // mScene.RequestUpdate(Scene::UpdateFlag::Meshes);
    mScene.RequestUpdate(Scene::UpdateFlag::Objects);
    // mScene.RequestUpdateAll();
}

SceneKey SceneEditor::EmplaceObject(std::optional<SceneKey> mesh)
{
    if (mesh)
        ASSERT(mScene.Meshes.count(*mesh) != 0);

    auto [key, obj] = mScene.EmplaceObject();

    obj.Mesh = mesh;

    return key;
}

SceneKey SceneEditor::DuplicateObject(SceneKey obj)
{
    auto &oldObj = GetObject(obj);

    auto [key, newObj] = mScene.EmplaceObject();

    newObj = oldObj;

    return key;
}

void SceneEditor::EraseObject(SceneKey key)
{
    mScene.Objects.erase(key);
}

void SceneEditor::UpdateTransforms(SceneGraphNode *rootNode)
{
    ASSERT(rootNode != nullptr);

    rootNode->UpdateTransforms(mScene);

    mScene.RequestUpdate(Scene::UpdateFlag::Objects);
}

void SceneEditor::LoadModel(const ModelConfig &config)
{
    mAssetManager.LoadModel(config);
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
    ASSERT(mNodeOpData.SrcParent != mNodeOpData.DstParent);

    auto &srcChildren = mNodeOpData.SrcParent->GetChildren();
    auto &dstChildren = mNodeOpData.DstParent->GetChildren();

    // Move to dst, erase from src:
    auto iter = mNodeOpData.GetSourceNodeIterator();

    dstChildren.push_back(std::move(*iter));
    srcChildren.erase(iter);
}

static void CopyNodeTree(SceneEditor &editor, SceneGraphNode &source,
                         SceneGraphNode &target)
{
    auto &newNode = [&]() -> SceneGraphNode & {
        if (source.IsLeaf())
        {
            auto key = editor.DuplicateObject(source.GetObjectKey());
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
            CopyNodeTree(editor, *child, newNode);
        }
    }
}

void SceneEditor::HandleNodeCopy()
{
    auto &oldNode = mNodeOpData.GetSourceNode();
    CopyNodeTree(*this, oldNode, *mNodeOpData.DstParent);
}

void SceneEditor::HandleNodeDelete()
{
    auto &children = mNodeOpData.SrcParent->GetChildren();
    auto it = mNodeOpData.GetSourceNodeIterator();

    children.erase(it);
}

std::pair<SceneKey, SceneGraphNode &> SceneEditor::EmplacePrefab(
    std::optional<SceneKey> meshKey)
{
    auto key = mPrefabKeyGenerator.Get();

    ASSERT(mPrefabs.count(key) == 0);

    if (meshKey)
        mPrefabs.emplace(key, SceneGraphNode(*this, *meshKey));
    else
        mPrefabs.emplace(key, SceneGraphNode(*this));

    return {key, mPrefabs.at(key)};
}

static void InstancePrefabImpl(SceneEditor &editor, SceneGraphNode &source,
                               SceneGraphNode &target)
{
    auto &newNode = [&]() -> SceneGraphNode & {
        if (source.IsLeaf())
        {
            // auto key = editor.DuplicateObject(source.GetObjectKey());
            auto key = editor.EmplaceObject(source.GetObjectKey());

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
            InstancePrefabImpl(editor, *child, newNode);
        }
    }
}

void SceneEditor::InstancePrefab(SceneKey prefabId)
{
    ASSERT(mPrefabs.count(prefabId) != 0);

    auto &prefabRoot = mPrefabs.at(prefabId);

    InstancePrefabImpl(*this, prefabRoot, GraphRoot);
    UpdateTransforms(&GraphRoot);
}