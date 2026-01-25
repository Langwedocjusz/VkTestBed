#include "SceneGraph.h"

#include "Vassert.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

SceneGraphNode::SceneGraphNode(Scene *scene) : mScene(scene)
{
    mPayload = ChildrenArray{};
}

SceneGraphNode::SceneGraphNode(Scene *scene, SceneKey objKey) : mScene(scene)
{
    mPayload = objKey;
}

SceneGraphNode::SceneGraphNode(SceneKey meshKey)
{
    mPayload = meshKey;
}

SceneGraphNode::SceneGraphNode(SceneGraphNode &&other) noexcept
    : Parent(other.Parent), Translation(other.Translation), Rotation(other.Rotation),
      Scale(other.Scale), Name(other.Name), mScene(other.mScene),
      mPayload(std::move(other.mPayload))
{
}

SceneGraphNode::~SceneGraphNode()
{
    if (mScene && IsLeaf())
    {
        // Remove object, the node pointed to:
        mScene->Objects.erase(GetObjectKey());
    }
}

bool SceneGraphNode::IsLeaf() const
{
    return std::holds_alternative<SceneKey>(mPayload);
}

SceneKey SceneGraphNode::GetObjectKey() const
{
    vassert(IsLeaf(), "Only leaf nodes hold object keys!");

    return std::get<SceneKey>(mPayload);
}

SceneGraphNode::ChildrenArray &SceneGraphNode::GetChildren()
{
    vassert(!IsLeaf(), "Leaf nodes have no children!");

    return std::get<ChildrenArray>(mPayload);
}

const SceneGraphNode::ChildrenArray &SceneGraphNode::GetChildrenConst() const
{
    vassert(!IsLeaf(), "Leaf nodes have no children!");

    return std::get<ChildrenArray>(mPayload);
}

SceneGraphNode &SceneGraphNode::EmplaceChild()
{
    GetChildren().push_back(std::make_unique<SceneGraphNode>(mScene));

    auto &child = *GetChildren().back();
    child.Parent = this;

    return child;
}

SceneGraphNode &SceneGraphNode::EmplaceChild(SceneKey key)
{
    GetChildren().push_back(std::make_unique<SceneGraphNode>(mScene, key));

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
    vassert(IsLeaf() == false);

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
    auto translation = glm::translate(glm::mat4(1.0f), Translation);
    auto rotation = glm::toMat4(glm::quat(Rotation));
    auto scale = glm::scale(glm::mat4(1.0f), Scale);

    return translation * rotation * scale;
}

glm::mat4 SceneGraphNode::GetAggregateTransform(glm::mat4 current)
{
    if (Parent)
    {
        return Parent->GetAggregateTransform(GetTransform() * current);
    }
    else
    {
        return GetTransform() * current;
    }
}

void SceneGraphNode::UpdateTransforms(Scene &scene, glm::mat4 current)
{
    UpdateTransformsImpl(scene, current);
    scene.RecalculateAABB();
}

void SceneGraphNode::UpdateTransformsImpl(Scene &scene, glm::mat4 current)
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