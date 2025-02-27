#include "SceneEditor.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/quaternion.hpp>

// SceneGraphNode Implementation:

SceneGraphNode::SceneGraphNode()
{
    Payload = ChildrenArray{};
}

SceneGraphNode::SceneGraphNode(SceneKey key)
{
    Payload = key;
}

bool SceneGraphNode::IsLeaf()
{
    return std::holds_alternative<SceneKey>(Payload);
}

SceneKey SceneGraphNode::GetObjectKey()
{
    if (!IsLeaf())
        throw std::logic_error("Only leaf nodes hold object keys!");

    return std::get<SceneKey>(Payload);
}

SceneGraphNode::ChildrenArray &SceneGraphNode::GetChildren()
{
    if (IsLeaf())
        throw std::logic_error("Leaf nodes have no children!");

    return std::get<ChildrenArray>(Payload);
}

SceneGraphNode &SceneGraphNode::EmplaceChild()
{
    GetChildren().push_back(std::make_unique<SceneGraphNode>());

    auto &child = *GetChildren().back();
    child.Parent = this;

    return child;
}

SceneGraphNode &SceneGraphNode::EmplaceChild(SceneKey key)
{
    GetChildren().push_back(std::make_unique<SceneGraphNode>(key));

    auto &child = *GetChildren().back();
    child.Parent = this;

    return child;
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

// SceneEditor Implementation:

SceneEditor::SceneEditor(Scene &scene) : mScene(scene), mAssetManager(scene)
{
    mScene.RequestUpdateAll();
}

void SceneEditor::OnUpdate()
{
    mAssetManager.OnUpdate();
}

const SceneMesh &SceneEditor::GetMesh(SceneKey key)
{
    assert(mScene.Meshes.count(key) != 0);

    return mScene.Meshes[key];
}

const SceneMaterial &SceneEditor::GetMaterial(SceneKey key)
{
    assert(mScene.Materials.count(key) != 0);

    return mScene.Materials[key];
}

const SceneObject &SceneEditor::GetObject(SceneKey key)
{
    assert(mScene.Objects.count(key) != 0);

    return mScene.Objects[key];
}

Scene::Environment &SceneEditor::GetEnv()
{
    return mScene.Env;
}

void SceneEditor::EraseMesh(SceneKey mesh)
{
    mScene.Meshes.erase(mesh);
    // To-do: also erase all objects referencing it
    // To-do: update flags
}

SceneKey SceneEditor::EmplaceObject(std::optional<SceneKey> mesh)
{
    if (mesh)
        assert(mScene.Meshes.count(*mesh) != 0);

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
    assert(rootNode != nullptr);

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

void SceneEditor::RequestUpdate(Scene::UpdateFlag flag)
{
    mScene.RequestUpdate(flag);
}