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

void SceneGraphNode::AddChild()
{
    GetChildren().push_back(std::make_unique<SceneGraphNode>());
}

void SceneGraphNode::AddChild(size_t idx)
{
    GetChildren().push_back(std::make_unique<SceneGraphNode>(idx));
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

void Scene::ClearUpdateFlags()
{
    UpdateRequested = false;
    GlobalUpdate = false;
}