#include "SceneEditor.h"

#include "Scene.h"
#include "VertexLayout.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <optional>
#include <ranges>
#include <string>

#include <format>
#include <glm/gtc/type_ptr.hpp>
#include <variant>

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

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

    // Draw nodes for all children of the scene root:
    using namespace std::views;

    for (const auto [id, child] : enumerate(scene.GraphRoot.GetChildren()))
        InstanceMenu(*child, &scene.GraphRoot, id);

    // Setup right-click context menu for adding things:
    AddInstancePopup(scene);

    // Add drop target spanning whole window
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
    if (parent != nullptr)
        assert(!parent->IsLeaf());
    else
        assert(!node.IsLeaf());

    // Assemble node name and flags:
    std::string nodeName = node.Name + "##" + std::to_string(childId);

    int32_t flags = ImGuiTreeNodeFlags_AllowOverlap;
    flags |= ImGuiTreeNodeFlags_OpenOnDoubleClick;
    flags |= ImGuiTreeNodeFlags_OpenOnArrow;

    if (node.IsLeaf())
        flags |= ImGuiTreeNodeFlags_Leaf;

    if (mSelectedNode == &node)
        flags |= ImGuiTreeNodeFlags_Selected;

    // Calculate position for the delete button (must happen before node is drawn):
    ImGuiContext &g = *GImGui;

    ImVec2 closeButtonPos = ImGui::GetCursorScreenPos();
    closeButtonPos.x +=
        ImGui::GetContentRegionAvail().x - g.Style.FramePadding.x - g.FontSize;

    // Drawing tree node:
    const bool nodeOpen = ImGui::TreeNodeEx(nodeName.c_str(), flags);

    // Handling associated drag/drop/clicked events:
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

        // Delete button:
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        ImGuiID id = window->GetID(nodeName.c_str());
        ImGuiID closeButtonId = ImGui::GetIDWithSeed("#CLOSE", nullptr, id);

        if (ImGui::CloseButton(closeButtonId, closeButtonPos))
        {
            mOpInfo = OpInfo{
                .RequestedOp = OpType::Delete,
                .Parent = parent,
                .ChildId = childId,
                .DstParent = nullptr,
            };
        }

        // Copy button:
        std::string copyButtonName = "+##" + nodeName;

        ImVec2 plusButtonPos = ImGui::GetCursorScreenPos();
        plusButtonPos.x = closeButtonPos.x - g.Style.FramePadding.x - g.FontSize;
        plusButtonPos.y -= g.Style.FramePadding.y;

        ImGui::SetCursorScreenPos(plusButtonPos);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

        if (ImGui::Button(copyButtonName.c_str()))
        {
            mOpInfo = OpInfo{
                .RequestedOp = OpType::Copy,
                .Parent = parent,
                .ChildId = childId,
                .DstParent = nullptr,
            };
        }

        ImGui::PopStyleColor();
    }

    // Recurse to also draw children nodes:
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

        bool isLeaf = node.IsLeaf();
        bool sameNode = &node == payload->Parent;

        bool validTarget = !isLeaf && !sameNode;

        if (validTarget)
        {
            mOpInfo = OpInfo{
                .RequestedOp = OpType::Move,
                .Parent = payload->Parent,
                .ChildId = payload->ChildId,
                .DstParent = &node,
            };
        }
    }
}

void SceneEditor::HandleNodeOp(Scene &scene)
{
    switch (mOpInfo.RequestedOp)
    {
    case OpType::Move: {
        HandleNodeMove();
        // To-do: this can be optimized to only update
        // affected nodes:
        scene.GraphRoot.UpdateTransforms(scene.Objects);
        scene.UpdateRequested = true;
        break;
    }
    case OpType::Delete: {
        HandleNodeDelete(scene);
        scene.UpdateRequested = true;
        break;
    }
    case OpType::Copy: {
        HandleNodeCopy(scene);
        scene.UpdateRequested = true;
        break;
    }
    case OpType::None: {
        break;
    }
    }

    mOpInfo.RequestedOp = OpType::None;
}

SceneGraphNode &SceneEditor::OpInfo::GetSourceNode()
{
    auto &children = Parent->GetChildren();
    auto &ptr = children[ChildId];
    return *ptr;
}

auto SceneEditor::OpInfo::GetSourceNodeIterator()
{
    auto &children = Parent->GetChildren();
    return children.begin() + ChildId;
}

void SceneEditor::HandleNodeMove()
{
    // We can assume src and dst are different, since
    // move operation wouldn't be scheduled otherwise.
    auto &srcChildren = mOpInfo.Parent->GetChildren();
    auto &dstChildren = mOpInfo.DstParent->GetChildren();

    // Move to dst, erase from src:
    auto iter = mOpInfo.GetSourceNodeIterator();

    dstChildren.push_back(std::move(*iter));
    srcChildren.erase(iter);
}

void SceneEditor::HandleNodeDelete(Scene &scene)
{
    auto &node = mOpInfo.GetSourceNode();

    // Reset selection if necessary:
    if (&node == mSelectedNode)
        mSelectedNode = nullptr;

    // Remove object, the node pointed to:
    scene.Objects[node.GetIndex()] = std::nullopt;

    // Erase the node:
    auto &children = mOpInfo.Parent->GetChildren();
    auto it = mOpInfo.GetSourceNodeIterator();

    children.erase(it);
}

void SceneEditor::HandleNodeCopy(Scene &scene)
{
    // Retrieve old node:
    auto &oldNode = mOpInfo.GetSourceNode();

    // Duplicate object:
    auto &obj = scene.Objects[oldNode.GetIndex()];
    scene.Objects.emplace_back(obj);

    // Duplicate node:
    auto id = scene.Objects.size() - 1;
    auto &newNode = mOpInfo.Parent->EmplaceChild(id);

    newNode.Translation = oldNode.Translation;
    newNode.Rotation = oldNode.Rotation;
    newNode.Scale = oldNode.Scale;
    newNode.Name = oldNode.Name;
}

void SceneEditor::AddInstancePopup(Scene &scene)
{
    if (ImGui::BeginPopupContextWindow())
    {
        ImGui::Text("Add:");
        ImGui::Separator();

        if (ImGui::Selectable("Empty Group"))
        {
            auto &groupNode = scene.GraphRoot.EmplaceChild();
            groupNode.Name = "Group";

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
                    .MaterialId = std::nullopt,
                    .Transform = glm::mat4(1.0f),
                });

                auto &newNode = scene.GraphRoot.EmplaceChild(idx);
                newNode.Name = provider.Name;

                scene.UpdateRequested = true;
            }
        }

        ImGui::EndPopup();
    }
}

void SceneEditor::DataMenu(Scene &scene)
{
    ImGui::Begin("Scene data", nullptr, ImGuiWindowFlags_NoFocusOnAppearing);

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

    if (ImGui::BeginTabItem("Materials"))
    {
        for (auto &mat : scene.Materials)
        {
            if (ImGui::TreeNodeEx(mat.Name.c_str()))
            {
                for (const auto &[key, value] : mat)
                {
                    std::string entry;
                    entry += key.Name;
                    entry += ": ";

                    // clang-format off
                    std::visit(
                        overloaded{
                            [&entry](Material::ImageSource img) {entry += img.Path.string();},
                            [&entry](float val) {entry += std::to_string(val);},
                            [&entry](glm::vec2 v) {entry += std::format("[{}, {}]", v.x, v.y);},
                            [&entry](glm::vec3 v) {entry += std::format("[{}, {}, {}]", v.x, v.y, v.z);},
                            [&entry](glm::vec4 v) {entry += std::format("[{}, {}, {}, {}]", v.x, v.y, v.z, v.w);},
                    }, value);
                    // clang-format on

                    ImGui::Text("%s", entry.c_str());
                }

                ImGui::TreePop();
            }
        }

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
                // To-do: this should only really update
                // objects associated with subtree rooted at mSelectedNode
                // This will require going along the path from root to
                // mSelectedNode and then propagating as usual.
                scene.GraphRoot.UpdateTransforms(scene.Objects);
                scene.UpdateRequested = true;
            }

            ImGui::TreePop();
        }

        if (mSelectedNode->IsLeaf())
        {
            std::string label = "##texlabel";

            auto &obj = scene.Objects[mSelectedNode->GetIndex()];

            if (obj->MaterialId.has_value())
            {
                label = scene.Materials[obj->MaterialId.value()].Name + label;
            }

            ImGui::Text("Material: ");
            ImGui::SameLine();

            if (ImGui::Selectable(label.c_str()))
                ImGui::OpenPopup("Select material:");

            if (ImGui::BeginPopup("Select material:"))
            {
                using namespace std::views;

                for (const auto [matId, mat] : enumerate(scene.Materials))
                {
                    if (ImGui::Selectable(mat.Name.c_str()))
                    {
                        obj->MaterialId = matId;
                        scene.UpdateRequested = true;
                    }
                }

                ImGui::EndPopup();
            }
        }
    }

    ImGui::End();
}
