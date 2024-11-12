#include "SceneEditor.h"

#include "Scene.h"
#include "VertexLayout.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>

#include <format>
#include <iostream>

#include <glm/gtc/type_ptr.hpp>

void SceneEditor::OnImGui(Scene &scene)
{
    DataMenu(scene);
    SceneHierarchyMenu(scene);
    ObjectPropertiesMenu(scene);
    HandleNodeOp(scene);
}

void SceneEditor::SceneHierarchyMenu(Scene &scene)
{
    ImGui::Begin("Scene hierarchy", nullptr, ImGuiWindowFlags_NoFocusOnAppearing);

    using namespace std::views;

    for (const auto [id, child] : enumerate(scene.GraphRoot.GetChildren()))
        InstanceMenu(*child, &scene.GraphRoot, id);

    AddInstancePopup(scene);

    ImGuiWindow *window = ImGui::GetCurrentWindow();

    if (ImGui::BeginDragDropTargetCustom(window->Rect(), window->ID))
    {
        HandleSceneDropPayload(scene.GraphRoot);
        ImGui::EndDragDropTarget();
    }

    ImGui::End();
}

void SceneEditor::InstanceMenu(SceneGraphNode &node, SceneGraphNode *parent,
                               int64_t childId)
{
    if (parent)
        assert(!parent->IsLeaf());
    else
        assert(!node.IsLeaf());

    std::string nodeName = node.Name + "##" + std::to_string(childId);

    int32_t flags = ImGuiTreeNodeFlags_AllowOverlap;
    flags |= ImGuiTreeNodeFlags_OpenOnDoubleClick;
    flags |= ImGuiTreeNodeFlags_OpenOnArrow;

    if (node.IsLeaf())
        flags |= ImGuiTreeNodeFlags_Leaf;

    if (mSelectedNode == &node)
        flags |= ImGuiTreeNodeFlags_Selected;

    // Calculate position for the delete button:
    ImGuiContext &g = *GImGui;
    float buttonSize = g.FontSize;
    ImVec2 closeButtonPos = ImGui::GetCursorScreenPos();
    closeButtonPos.x +=
        ImGui::GetContentRegionAvail().x - g.Style.FramePadding.x - buttonSize;

    // Drawing tree node:
    const bool nodeOpen = ImGui::TreeNodeEx(nodeName.c_str(), flags);

    // Handling all associated events:
    if (ImGui::IsItemClicked())
    {
        mSelectedNode = &node;
    }

    if (ImGui::BeginDragDropSource())
    {
        DragPayload payload{
            .Parent = parent,
            .ChildId = childId,
        };

        ImGui::SetDragDropPayload("SCENE_INSTANCE_PAYLOAD", &payload, sizeof(payload));
        ImGui::EndDragDropSource();
    }

    if (ImGui::BeginDragDropTarget())
    {
        HandleSceneDropPayload(node);
        ImGui::EndDragDropTarget();
    }

    // Drawing additional copy/delete ui on top:
    // To-do: also handle non-leaf nodes:
    if (node.IsLeaf())
    {
        ImGui::SameLine();

        ImGuiWindow *window = ImGui::GetCurrentWindow();
        ImGuiID id = window->GetID(nodeName.c_str());
        ImGuiID closeButtonId = ImGui::GetIDWithSeed("#CLOSE", nullptr, id);

        if (ImGui::CloseButton(closeButtonId, closeButtonPos))
        {
            mOpInfo = OpInfo{
                .RequestedOp = OpType::Delete,
                .Parent = parent,
                .Idx = childId,
                .DstParent = nullptr,
            };
        }

        ImVec2 plusButtonPos = ImGui::GetCursorScreenPos();
        plusButtonPos.x = closeButtonPos.x - g.Style.FramePadding.x - buttonSize;
        plusButtonPos.y -= g.Style.FramePadding.y;

        ImGui::SetCursorScreenPos(plusButtonPos);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

        std::string copyButtonName = "+##" + nodeName;

        if (ImGui::Button(copyButtonName.c_str()))
        {
            mOpInfo = OpInfo{
                .RequestedOp = OpType::Copy,
                .Parent = parent,
                .Idx = childId,
                .DstParent = nullptr,
            };
        }

        ImGui::PopStyleColor();
    }

    if (nodeOpen)
    {
        if (!node.IsLeaf())
        {
            using namespace std::views;

            for (const auto [idx, child] : enumerate(node.GetChildren()))
            {
                InstanceMenu(*child, &node, idx);
            }
        }

        ImGui::TreePop();
    }
}

void SceneEditor::HandleSceneDropPayload(SceneGraphNode &node)
{
    if (const auto imPayload = ImGui::AcceptDragDropPayload("SCENE_INSTANCE_PAYLOAD"))
    {
        auto payload = static_cast<DragPayload *>(imPayload->Data);

        bool validTarget = true;

        if (node.IsLeaf())
            validTarget = false;
        if (&node == payload->Parent)
            validTarget = false;

        if (validTarget)
        {
            mOpInfo = OpInfo{
                .RequestedOp = OpType::Move,
                .Parent = payload->Parent,
                .Idx = static_cast<int64_t>(payload->ChildId),
                .DstParent = &node,
            };
        }
    }
}

void SceneEditor::HandleNodeOp(Scene& scene)
{
    switch(mOpInfo.RequestedOp)
    {
        case OpType::Move:
        {
            HandleNodeMove();
            // To-do: this can be optimized to only update
            // affected nodes:
            scene.GraphRoot.UpdateTransforms(scene.Objects);
            scene.UpdateRequested = true;
            break;
        }
        case OpType::Delete:
        {
            HandleNodeDelete(scene);
            scene.UpdateRequested = true;
            break;
        }
        case OpType::Copy:
        {
            HandleNodeCopy(scene);
            scene.UpdateRequested = true;
            break;
        }
        case OpType::None:
        {
            break;
        }
    }

    mOpInfo.RequestedOp = OpType::None;
}

void SceneEditor::HandleNodeMove()
{
    auto &srcChildren = mOpInfo.Parent->GetChildren();
    auto &dstChildren = mOpInfo.DstParent->GetChildren();

    {
        auto iter = srcChildren.begin() + mOpInfo.Idx;
        dstChildren.push_back(std::move(*iter));
    }

    {
        auto iter = srcChildren.begin() + mOpInfo.Idx;
        srcChildren.erase(iter);
    }
}

void SceneEditor::HandleNodeDelete(Scene &scene)
{
    auto &children = mOpInfo.Parent->GetChildren();
    auto &node = children[mOpInfo.Idx];

    if (node.get() == mSelectedNode)
        mSelectedNode = nullptr;

    scene.Objects[node->GetIndex()] = std::nullopt;

    auto it = children.begin() + mOpInfo.Idx;
    children.erase(it);
}

void SceneEditor::HandleNodeCopy(Scene &scene)
{
    auto &children = mOpInfo.Parent->GetChildren();

    {
        //Duplicate object:
        auto &node = children[mOpInfo.Idx];
        auto& obj = scene.Objects[node->GetIndex()];
        scene.Objects.emplace_back(obj);
    }

    {
        //Duplicate node:
        auto id = scene.Objects.size() - 1;
        mOpInfo.Parent->AddChild(id);

        auto &oldNode = children[mOpInfo.Idx];
        auto &newNode = mOpInfo.Parent->GetChildren().back();

        newNode->Translation = oldNode->Translation;
        newNode->Rotation = oldNode->Rotation;
        newNode->Scale = oldNode->Scale;
        newNode->Name = oldNode->Name;
    }
}

void SceneEditor::AddInstancePopup(Scene &scene)
{
    if (ImGui::BeginPopupContextWindow())
    {
        ImGui::Text("Add:");
        ImGui::Separator();

        if (ImGui::Selectable("Empty Group"))
        {
            scene.GraphRoot.AddChild();
            scene.GraphRoot.GetChildren().back()->Name = "Group";

            scene.UpdateRequested = true;
        }

        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        ImGui::Text("Instance:");
        ImGui::Separator();

        using namespace std::views;

        for (const auto [id, provider] : enumerate(scene.GeoProviders))
        {
            std::string name = provider.Name + "##" + std::to_string(id);

            if (ImGui::Selectable(name.c_str()))
            {
                size_t idx = scene.EmplaceObject(SceneObject{
                    .GeometryId = id,
                    .TextureId = std::nullopt,
                    .Transform = glm::mat4(1.0f),
                });

                scene.GraphRoot.AddChild(idx);
                scene.GraphRoot.GetChildren().back()->Name = provider.Name;

                scene.UpdateRequested = true;
            }
        }

        ImGui::EndPopup();
    }
}

void SceneEditor::DataMenu(Scene &scene)
{
    ImGui::Begin("Scene data", nullptr, ImGuiWindowFlags_NoFocusOnAppearing);
    // ImGui::SameLine();
    auto size = ImVec2(ImGui::GetContentRegionAvail().x, 0.0f);

    if (ImGui::Button("Full reload", size))
    {
        scene.UpdateRequested = true;
        scene.GlobalUpdate = true;
    }

    ImGui::BeginTabBar("We");

    if (ImGui::BeginTabItem("Geometry"))
    {
        using namespace std::views;
        for (const auto [id, provider] : enumerate(scene.GeoProviders))
        {
            std::string nodeName = provider.Name + "##" + std::to_string(id);

            if (ImGui::TreeNodeEx(nodeName.c_str()))
            {
                std::string vertLayout = "Vertex Layout: ";

                for (auto type : provider.Layout.VertexLayout)
                    vertLayout += Vertex::ToString(type) + ", ";

                ImGui::Text("%s", vertLayout.c_str());

                ImGui::TreePop();
            }
        }

        AddProviderPopup(scene);
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Textures"))
    {
        for (auto &path : scene.Textures.data())
            ImGui::Selectable(path.c_str());

        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();

    ImGui::End();
}

void SceneEditor::AddProviderPopup([[maybe_unused]] Scene &scene)
{
    if (ImGui::BeginPopupContextWindow())
    {
        ImGui::Text("Add provider:");
        ImGui::EndPopup();
    }
}

static bool TransformWidget(SceneGraphNode &node)
{
    auto clampPositive = [](float &x) {
        if (x < 0.0f)
            x = 0.0f;
    };

    auto prev_trans = node.Translation;
    auto prev_rot = node.Rotation;
    auto prev_scl = node.Scale;

    auto trans_ptr = glm::value_ptr(node.Translation);
    auto rot_ptr = glm::value_ptr(node.Rotation);
    auto scl_ptr = glm::value_ptr(node.Scale);

    const float speed = 0.01f;

    ImGui::DragFloat3("Translation", trans_ptr, speed);
    ImGui::DragFloat3("Rotation", rot_ptr, speed);
    ImGui::DragFloat3("Scale", scl_ptr, speed);

    clampPositive(node.Scale.x);
    clampPositive(node.Scale.y);
    clampPositive(node.Scale.z);

    bool changed = false;
    changed = changed || (prev_trans != node.Translation);
    changed = changed || (prev_rot != node.Rotation);
    changed = changed || (prev_scl != node.Scale);

    return changed;
}

void SceneEditor::ObjectPropertiesMenu(Scene &scene)
{
    ImGui::Begin("Object properties", nullptr, ImGuiWindowFlags_NoFocusOnAppearing);

    if (mSelectedNode)
    {
        if (ImGui::TreeNodeEx("Transform"))
        {
            if (TransformWidget(*mSelectedNode))
            {
                mSelectedNode->UpdateTransforms(scene.Objects);
                scene.UpdateRequested = true;
            }

            ImGui::TreePop();
        }

        if (mSelectedNode->IsLeaf())
        {
            std::string label = "##texlabel";

            auto &obj = scene.Objects[mSelectedNode->GetIndex()];

            if (obj->TextureId.has_value())
            {
                label = scene.Textures.data()[obj->TextureId.value()] + label;
            }

            ImGui::Text("Texture: ");
            ImGui::SameLine();

            if (ImGui::Selectable(label.c_str()))
                ImGui::OpenPopup("Select texture:");

            if (ImGui::BeginPopup("Select texture:"))
            {
                using namespace std::views;

                for (const auto [texId, path] : enumerate(scene.Textures.data()))
                {
                    if (ImGui::Selectable(path.c_str()))
                    {
                        obj->TextureId = texId;
                        scene.UpdateRequested = true;
                    }
                }

                ImGui::EndPopup();
            }
        }
    }

    ImGui::End();
}
