#include "Scene.h"
#include <stdexcept>
#include <variant>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/quaternion.hpp>

SceneGraphNode::SceneGraphNode()
{
    Payload = ChildrenArray{};
}

SceneGraphNode::SceneGraphNode(size_t instanceId)
{
    Payload = instanceId;
}

bool SceneGraphNode::IsLeaf()
{
    return std::holds_alternative<ObjectId>(Payload);
}

size_t SceneGraphNode::GetIndex()
{
    if (!IsLeaf())
        throw std::logic_error("Only leaf nodes hold indices!");

    return std::get<ObjectId>(Payload);
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

SceneGraphNode &SceneGraphNode::EmplaceChild(size_t idx)
{
    GetChildren().push_back(std::make_unique<SceneGraphNode>(idx));

    auto &child = *GetChildren().back();
    child.Parent = this;

    return child;
}

glm::mat4 SceneGraphNode::GetTransform()
{
    return glm::translate(glm::mat4(1.0f), Translation) *
           glm::toMat4(glm::quat(Rotation)) * glm::scale(glm::mat4(1.0f), Scale);
}

void SceneGraphNode::UpdateTransforms(ObjectArray &objs, glm::mat4 current)
{
    if (IsLeaf())
    {
        auto &obj = objs[GetIndex()];

        if (!obj.has_value())
            throw std::logic_error("Scene graph holds invalid object id.");

        obj.value().Transform = current * GetTransform();
    }

    else
    {
        for (auto &child : GetChildren())
        {
            child->UpdateTransforms(objs, current * GetTransform());
        }
    }
}

size_t Scene::EmplaceObject(SceneObject obj)
{
    // To-do: maybe keep a separate list of nullopts
    // to do this in less than O(N)
    for (size_t i = 0; i < Objects.size(); i++)
    {
        if (Objects[i] == std::nullopt)
        {
            Objects[i] = obj;
            return i;
        }
    }

    Objects.emplace_back(obj);
    return Objects.size() - 1;
}

std::map<SceneKey, SceneMesh> &Scene::Meshes()
{
    return mMeshes;
}

std::map<SceneKey, Material> &Scene::Materials()
{
    return mMaterials;
}

void Scene::EraseMesh(SceneKey key)
{
    mMeshes.erase(key);
}

void Scene::EraseMaterial(SceneKey key)
{
    mMaterials.erase(key);
}

SceneMesh &Scene::EmplaceMesh()
{
    auto key = mMeshKeyGenerator.Get();

    if (mMeshes.count(key) != 0)
        throw std::runtime_error("Cannot emplace mesh, id already in use!");

    mMeshes[key] = SceneMesh{};
    return mMeshes[key];
}

Material &Scene::EmplaceMaterial()
{
    auto key = mMaterialKeyGenerator.Get();

    if (mMaterials.count(key) != 0)
        throw std::runtime_error("Cannot emplace mesh, id already in use!");

    mMaterials[key] = Material{};
    return mMaterials[key];
}

std::pair<SceneKey, Material &> Scene::EmplaceMaterial2()
{
    auto key = mMaterialKeyGenerator.Get();

    if (mMaterials.count(key) != 0)
        throw std::runtime_error("Cannot emplace mesh, id already in use!");

    mMaterials[key] = Material{};

    return {key, mMaterials[key]};
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

void Scene::RequestMaterialUpdate()
{
    mUpdateFlags.Set(Update::Materials);
    mUpdateFlags.Set(Update::MeshMaterials);
}

void Scene::RequestGeometryUpdate()
{
    mUpdateFlags.Set(Update::MeshGeo);
    mUpdateFlags.Set(Update::MeshMaterials);
    mUpdateFlags.Set(Update::Instances);
}

void Scene::RequestMeshMaterialUpdate()
{
    mUpdateFlags.Set(Update::MeshMaterials);
}

void Scene::RequestInstanceUpdate()
{
    mUpdateFlags.Set(Update::Instances);
}

void Scene::RequestEnvironmentUpdate()
{
    mUpdateFlags.Set(Update::Environment);
}

bool Scene::UpdateMaterials()
{
    return mUpdateFlags[Update::Materials];
}

bool Scene::UpdateGeometry()
{
    return mUpdateFlags[Update::MeshGeo];
}

bool Scene::UpdateMeshMaterials()
{
    return mUpdateFlags[Update::MeshMaterials];
}

bool Scene::UpdateInstances()
{
    return mUpdateFlags[Update::Instances];
}

bool Scene::UpdateEnvironment()
{
    return mUpdateFlags[Update::Environment];
}