#include "SceneEditor.h"

#include "CppUtils.h"
#include "ImGuiUtils.h"
#include "Primitives.h"
#include "Scene.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <glm/gtc/type_ptr.hpp>

#include <format>
#include <iostream>
#include <optional>
#include <ranges>
#include <string>
#include <variant>

static const char *PAYLOAD_STRING = "SCENE_INSTANCE_PAYLOAD";

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

void SceneEditor::OnInit(Scene &scene)
{
    mHdriBrowser.AddExtensionToFilter(".exr");

    mHdriBrowser.SetCallbackFn([&]() {
        scene.Env.HdriPath = mHdriBrowser.ChosenFile;
        scene.RequestEnvironmentUpdate();
    });

    mHdriBrowser.SetCheckFn([](const std::filesystem::path &path) {
        return std::filesystem::is_regular_file(path);
    });

    {
        auto &mesh = scene.EmplaceMesh();
        mesh.Name = "Colored Cube";
        mesh.Geometry = primitive::ColoredCube;
    }

    {
        auto [key, mat] = scene.EmplaceMaterial2();

        mat.Name = "Test Material";

        mat[Material::Albedo] = Material::ImageSource{
            .Path = "./assets/textures/texture.jpg",
            .Channel = Material::ImageChannel::RGBA,
        };

        auto &mesh = scene.EmplaceMesh();
        mesh.Name = "Textured Cube";
        mesh.Geometry = primitive::TexturedCube;
        mesh.Materials.push_back(key);
    }

    scene.RequestFullUpdate();
}

void SceneEditor::OnImGui(Scene &scene)
{
    DataMenu(scene);
    SceneHierarchyMenu(scene);
    ObjectPropertiesMenu(scene);
}

void SceneEditor::SceneHierarchyMenu(Scene &scene)
{
    ImGui::Begin("Scene hierarchy", nullptr, ImGuiWindowFlags_NoFocusOnAppearing);

    // Draw nodes for all children of the scene root:
    using namespace std::views;

    for (const auto [id, child] : enumerate(scene.GraphRoot.GetChildren()))
        InstanceGui(*child, &scene.GraphRoot, id);

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

    // Handle node operations if any were scheduled:
    HandleNodeOp(scene);

    // Handle model loading if it was scheduled:
    mModelLoader.OnImGui(scene);
}

void SceneEditor::InstanceGui(SceneGraphNode &node, SceneGraphNode *parent,
                              int64_t childId)
{
    // Sanity check:
    if (parent != nullptr)
        assert(!parent->IsLeaf());
    else
        assert(!node.IsLeaf());

    // ImGui context for later:
    ImGuiContext &g = *GImGui;

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
    ImVec2 closeButtonPos = ImGui::GetCursorScreenPos();
    closeButtonPos.x +=
        ImGui::GetContentRegionAvail().x - g.Style.FramePadding.x - g.FontSize;

    // Draw the tree node:
    const bool nodeOpen = ImGui::TreeNodeEx(nodeName.c_str(), flags);

    // Handle associated drag/drop/clicked events:
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

        ImGui::SetDragDropPayload(PAYLOAD_STRING, &payload, sizeof(payload));
        ImGui::EndDragDropSource();
    }

    if (ImGui::BeginDragDropTarget())
    {
        HandleSceneDropPayload(node);
        ImGui::EndDragDropTarget();
    }

    // Draw additional copy/delete ui on top:
    // To-do: also handle non-leaf nodes:
    if (node.IsLeaf())
    {
        ImGui::SameLine();

        // Delete button:
        if (imutils::CloseButton(nodeName, closeButtonPos))
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
                InstanceGui(*child, &node, idx);
            }
        }

        ImGui::TreePop();
    }
}

void SceneEditor::HandleSceneDropPayload(SceneGraphNode &node)
{
    if (const auto imPayload = ImGui::AcceptDragDropPayload(PAYLOAD_STRING))
    {
        auto payload = static_cast<DragPayload *>(imPayload->Data);

        bool sameNode = &node == payload->Parent;

        bool validTarget = !node.IsLeaf() && !sameNode;

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
        scene.GraphRoot.UpdateTransforms(scene);
        scene.RequestObjectUpdate();
        break;
    }
    case OpType::Delete: {
        HandleNodeDelete(scene);
        scene.RequestObjectUpdate();
        break;
    }
    case OpType::Copy: {
        HandleNodeCopy(scene);
        scene.RequestObjectUpdate();
        break;
    }
    case OpType::None: {
        break;
    }
    }

    mOpInfo.RequestedOp = OpType::None;
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
    scene.EraseObject(node.GetObjectKey());

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
    auto &obj = scene.GetObject(oldNode.GetObjectKey());
    auto id = scene.EmplaceObject(obj);

    // Duplicate node:
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

            scene.RequestObjectUpdate();
        }

        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        ImGui::Text("Instance:");
        ImGui::Separator();

        for (auto &[key, mesh] : scene.Meshes())
        {
            std::string name = mesh.Name + "##" + std::to_string(key);

            if (ImGui::Selectable(name.c_str()))
            {
                auto [idx, obj] = scene.EmplaceObject2();
                obj.MeshId = key;
                obj.Transform = glm::mat4(1.0f);

                auto &newNode = scene.GraphRoot.EmplaceChild(idx);
                newNode.Name = mesh.Name;

                scene.RequestObjectUpdate();
            }
        }

        // To-do: maybe move this to the data menu
        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        if (ImGui::Selectable("Load Model"))
        {
            mModelLoader.TriggerLoad();
        }

        ImGui::EndPopup();
    }
}

void SceneEditor::DataMenu(Scene &scene)
{
    ImGui::Begin("Scene data", nullptr, ImGuiWindowFlags_NoFocusOnAppearing);

    auto buttonSize = ImVec2(ImGui::GetContentRegionAvail().x, 0.0f);

    if (ImGui::Button("Full reload", buttonSize))
    {
        scene.RequestFullUpdate();
    }

    ImGui::BeginTabBar("We");

    if (ImGui::BeginTabItem("Meshes"))
    {
        MeshesTab(scene);
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Materials"))
    {
        MaterialsTab(scene);
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Environment"))
    {
        EnvironmentTab(scene);
        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();

    ImGui::End();
}

void SceneEditor::MeshesTab(Scene &scene)
{
    using namespace std::views;

    std::optional<SceneKey> keyToDelete;

    for (auto &[meshKey, mesh] : scene.Meshes())
    {
        std::string nodeName = mesh.Name + "##" + std::to_string(meshKey);

        ImGuiContext &g = *GImGui;

        ImVec2 closeButtonPos = ImGui::GetCursorScreenPos();
        closeButtonPos.x +=
            ImGui::GetContentRegionAvail().x - g.Style.FramePadding.x - g.FontSize;

        bool nodeOpen = ImGui::TreeNodeEx(nodeName.c_str());

        if (imutils::CloseButton(nodeName, closeButtonPos))
        {
            keyToDelete = meshKey;
        }

        if (nodeOpen)
        {
            //// Display vertex layout:
            // std::string vertLayout = "Vertex Layout: ";
            //
            // for (auto type : mesh.GeoProvider.Layout.VertexLayout)
            //     vertLayout += Vertex::ToString(type) + ", ";
            //
            // ImGui::Text("%s", vertLayout.c_str());
            //
            // ImGui::Separator();

            // Material editing gui:
            ImGui::Text("Materials:");

            // To-do: this is kind of ugly:
            // static used here because this value is accessed across
            // two different calls of this function.
            static SceneKey *idToChange = nullptr;

            for (const auto [num, id] : enumerate(mesh.Materials))
            {
                const std::string suffix = "##mat" + mesh.Name + std::to_string(num);
                std::string matName = scene.GetMaterial(id).Name + suffix;

                ImGui::Text("Material %lu: ", num);
                ImGui::SameLine();

                if (ImGui::Selectable(matName.c_str()))
                {
                    idToChange = &id;
                    ImGui::OpenPopup("Select material:");
                }
                // To-do: adding new materials
            }

            if (ImGui::BeginPopup("Select material:"))
            {
                if (idToChange != nullptr)
                {
                    for (auto &[id, mat] : scene.Materials())
                    {
                        if (ImGui::Selectable(mat.Name.c_str()))
                        {
                            *idToChange = id;
                            scene.RequestMeshMaterialUpdate();
                        }
                    }
                }

                ImGui::EndPopup();
            }

            ImGui::TreePop();
        }
    }

    AddProviderPopup(scene);

    if (keyToDelete)
    {
        scene.EraseMesh(*keyToDelete);
        scene.RequestMeshUpdate();
    }
}

void SceneEditor::MaterialsTab(Scene &scene)
{
    for (auto &[_, mat] : scene.Materials())
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
}

void SceneEditor::EnvironmentTab(Scene &scene)
{
    if (ImGui::Checkbox("Directional light", &scene.Env.DirLightOn))
    {
        scene.RequestEnvironmentUpdate();
    }

    static float phi = 2.359f;
    static float theta = 1.650f;

    ImGui::SliderFloat("Azimuth", &phi, 0.0f, 6.28f);
    ImGui::SliderFloat("Altitude", &theta, 0.0f, 3.14f);

    const float cT = cos(theta), sT = sin(theta);
    const float cP = cos(phi), sP = sin(phi);

    glm::vec3 newDir(cP * sT, cT, sP * sT);

    if (newDir != scene.Env.LightDir)
    {
        scene.Env.LightDir = newDir;
        scene.RequestEnvironmentUpdate();
    }

    ImGui::Text("Hdri path:");

    ImGui::SameLine();

    std::string selText = scene.Env.HdriPath ? (*scene.Env.HdriPath).string() : "";
    selText += "##HDRI";

    if (ImGui::Selectable(selText.c_str()))
    {
        mOpenHdriPopup = true;
    }

    auto size = ImVec2(ImGui::GetContentRegionAvail().x, 0.0f);

    if (ImGui::Button("Clear hdri", size))
    {
        scene.Env.HdriPath = std::nullopt;
        scene.RequestEnvironmentUpdate();
    }

    // Implementation of the popup for hdri selection:
    SelectHdriPopup();
}

void SceneEditor::SelectHdriPopup()
{
    const std::string popupName = "Load hdri...";

    if (mOpenHdriPopup)
    {
        ImGui::OpenPopup(popupName.c_str());
        mOpenHdriPopup = false;
    }

    mHdriBrowser.ImGuiLoadPopup(popupName, mHdriStillOpen);

    mHdriStillOpen = true;
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

    auto prevTrans = node.Translation;
    auto prevRot = node.Rotation;
    auto prevScl = node.Scale;

    auto transPtr = glm::value_ptr(node.Translation);
    auto rotPtr = glm::value_ptr(node.Rotation);
    auto sclPtr = glm::value_ptr(node.Scale);

    const float speed = 0.01f;

    ImGui::DragFloat3("Translation", transPtr, speed);
    ImGui::DragFloat3("Rotation", rotPtr, speed);
    ImGui::DragFloat3("Scale", sclPtr, speed);

    clampPositive(node.Scale.x);
    clampPositive(node.Scale.y);
    clampPositive(node.Scale.z);

    bool changed = false;
    changed = changed || (prevTrans != node.Translation);
    changed = changed || (prevRot != node.Rotation);
    changed = changed || (prevScl != node.Scale);

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
                scene.GraphRoot.UpdateTransforms(scene);
                scene.RequestObjectUpdate();
            }

            ImGui::TreePop();
        }
    }

    ImGui::End();
}