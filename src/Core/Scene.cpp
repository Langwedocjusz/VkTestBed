#include "Scene.h"
#include <stdexcept>
#include <variant>

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
        auto &obj = scene.GetObject(GetObjectKey());

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

// Scene Implementation:

SceneMesh &Scene::EmplaceMesh()
{
    auto key = mMeshKeyGenerator.Get();

    if (mMeshes.count(key) != 0)
        throw std::runtime_error("Cannot emplace mesh, key already in use!");

    mMeshes[key] = SceneMesh{};
    return mMeshes[key];
}

Material &Scene::EmplaceMaterial()
{
    auto key = mMaterialKeyGenerator.Get();

    if (mMaterials.count(key) != 0)
        throw std::runtime_error("Cannot emplace material, key already in use!");

    mMaterials[key] = Material{};
    return mMaterials[key];
}

SceneObject &Scene::EmplaceObject()
{
    auto key = mObjectKeyGenerator.Get();

    if (mObjects.count(key) != 0)
        throw std::runtime_error("Cannot emplace object, key already in use!");

    mObjects[key] = SceneObject{};
    return mObjects[key];
}

std::pair<SceneKey, Material &> Scene::EmplaceMaterial2()
{
    auto key = mMaterialKeyGenerator.Get();

    if (mMaterials.count(key) != 0)
        throw std::runtime_error("Cannot emplace material, key already in use!");

    mMaterials[key] = Material{};

    return {key, mMaterials[key]};
}

std::pair<SceneKey, SceneObject &> Scene::EmplaceObject2()
{
    auto key = mObjectKeyGenerator.Get();

    if (mObjects.count(key) != 0)
        throw std::runtime_error("Cannot emplace object, key already in use!");

    mObjects[key] = SceneObject{};

    return {key, mObjects[key]};
}

SceneKey Scene::EmplaceObject(const SceneObject &obj)
{
    auto key = mObjectKeyGenerator.Get();

    if (mObjects.count(key) != 0)
        throw std::runtime_error("Cannot emplace object, key already in use!");

    mObjects[key] = obj;

    return key;
}

void Scene::EraseMesh(SceneKey key)
{
    mMeshes.erase(key);
}

void Scene::EraseMaterial(SceneKey key)
{
    mMaterials.erase(key);
}

void Scene::EraseObject(SceneKey key)
{
    mObjects.erase(key);
}

SceneMesh &Scene::GetMesh(SceneKey key)
{
    return mMeshes[key];
}

Material &Scene::GetMaterial(SceneKey key)
{
    return mMaterials[key];
}

SceneObject &Scene::GetObject(SceneKey key)
{
    return mObjects[key];
}

bool Scene::UpdateRequested()
{
    return mUpdateFlags.Any();
}

void Scene::ClearUpdateFlags()
{
    mUpdateFlags.Clear();
}

void Scene::RequestFullUpdate()
{
    mUpdateFlags.SetAll();
}

void Scene::RequestMeshUpdate()
{
    mUpdateFlags.Set(Update::MeshGeo);
    mUpdateFlags.Set(Update::MeshMaterials);
    mUpdateFlags.Set(Update::Instances);
}

void Scene::RequestMaterialUpdate()
{
    mUpdateFlags.Set(Update::Materials);
    mUpdateFlags.Set(Update::MeshMaterials);
}

void Scene::RequestMeshMaterialUpdate()
{
    mUpdateFlags.Set(Update::MeshMaterials);
}

void Scene::RequestObjectUpdate()
{
    mUpdateFlags.Set(Update::Instances);
}

void Scene::RequestEnvironmentUpdate()
{
    mUpdateFlags.Set(Update::Environment);
}

bool Scene::UpdateMeshes()
{
    return mUpdateFlags[Update::MeshGeo];
}

bool Scene::UpdateMaterials()
{
    return mUpdateFlags[Update::Materials];
}

bool Scene::UpdateMeshMaterials()
{
    return mUpdateFlags[Update::MeshMaterials];
}

bool Scene::UpdateObjects()
{
    return mUpdateFlags[Update::Instances];
}

bool Scene::UpdateEnvironment()
{
    return mUpdateFlags[Update::Environment];
}