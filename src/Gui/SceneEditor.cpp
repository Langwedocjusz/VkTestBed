#include "SceneEditor.h"

#include "CppUtils.h"
#include "Primitives.h"
#include "Scene.h"
#include "VertexLayout.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <glm/gtc/type_ptr.hpp>

#include <format>
#include <iostream>
#include <optional>
#include <ranges>
#include <string>
#include <variant>

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
        auto &mat = scene.Materials.emplace_back();

        mat.Name = "Test Material";

        mat[Material::Albedo] = Material::ImageSource{
            .Path = "./assets/textures/texture.jpg",
            .Channel = Material::ImageChannel::RGBA,
        };
    }

    {
        auto &mesh = scene.Meshes.emplace_back();
        mesh.Name = "Colored Cube";
        mesh.GeoProvider = primitive::ColoredCube(glm::vec3(0.5f));
    }

    {
        auto &mesh = scene.Meshes.emplace_back();
        mesh.Name = "Textured Cube";
        mesh.GeoProvider = primitive::TexturedCube();
        mesh.MaterialIds.push_back(0);
    }

    scene.RequestFullUpdate();
}

void SceneEditor::OnImGui(Scene &scene)
{
    DataMenu(scene);
    SceneHierarchyMenu(scene);
    ObjectPropertiesMenu(scene);
    HandleNodeOp(scene);
    SelectHdri();

    mModelLoader.OnImGui(scene);
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
        scene.RequestInstanceUpdate();
        break;
    }
    case OpType::Delete: {
        HandleNodeDelete(scene);
        scene.RequestInstanceUpdate();
        break;
    }
    case OpType::Copy: {
        HandleNodeCopy(scene);
        scene.RequestInstanceUpdate();
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

            scene.RequestInstanceUpdate();
        }

        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        ImGui::Text("Instance:");
        ImGui::Separator();

        using namespace std::views;

        for (const auto [id, mesh] : enumerate(scene.Meshes))
        {
            std::string name = mesh.Name + "##" + std::to_string(id);

            if (ImGui::Selectable(name.c_str()))
            {
                size_t idx = scene.EmplaceObject(SceneObject{
                    .MeshId = id,
                    .Transform = glm::mat4(1.0f),
                });

                auto &newNode = scene.GraphRoot.EmplaceChild(idx);
                newNode.Name = mesh.Name;

                scene.RequestInstanceUpdate();
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

    auto size = ImVec2(ImGui::GetContentRegionAvail().x, 0.0f);

    if (ImGui::Button("Full reload", size))
    {
        scene.RequestMaterialUpdate();
        scene.RequestGeometryUpdate();
    }

    ImGui::BeginTabBar("We");

    if (ImGui::BeginTabItem("Meshes"))
    {
        using namespace std::views;

        std::optional<int64_t> idToDelete;

        for (const auto [meshId, mesh] : enumerate(scene.Meshes))
        {
            std::string nodeName = mesh.Name + "##" + std::to_string(meshId);

            ImGuiContext &g = *GImGui;

            ImVec2 closeButtonPos = ImGui::GetCursorScreenPos();
            closeButtonPos.x +=
                ImGui::GetContentRegionAvail().x - g.Style.FramePadding.x - g.FontSize;

            bool nodeOpen = ImGui::TreeNodeEx(nodeName.c_str());

            ImGuiWindow *window = ImGui::GetCurrentWindow();
            ImGuiID id = window->GetID(nodeName.c_str());
            ImGuiID closeButtonId = ImGui::GetIDWithSeed("#CLOSE", nullptr, id);

            if (ImGui::CloseButton(closeButtonId, closeButtonPos))
            {
                idToDelete = meshId;
            }

            if (nodeOpen)
            {
                // Display vertex layout:
                std::string vertLayout = "Vertex Layout: ";

                for (auto type : mesh.GeoProvider.Layout.VertexLayout)
                    vertLayout += Vertex::ToString(type) + ", ";

                ImGui::Text("%s", vertLayout.c_str());

                ImGui::Separator();

                // Material editing gui:
                ImGui::Text("Materials:");

                // To-do: this is kind of ugly:
                static size_t *idToChange = nullptr;

                for (const auto [num, id] : enumerate(mesh.MaterialIds))
                {
                    const std::string suffix = "##mat" + mesh.Name + std::to_string(num);
                    std::string matName = scene.Materials[id].Name + suffix;

                    ImGui::Text("Material %lu: ", num);
                    ImGui::SameLine();

                    if (ImGui::Selectable(matName.c_str()))
                    {
                        idToChange = &id;
                        ImGui::OpenPopup("Select material:");
                    }
                    // To-do: adding new maerials
                }

                if (ImGui::BeginPopup("Select material:") && idToChange != nullptr)
                {
                    for (const auto [id, mat] : enumerate(scene.Materials))
                    {
                        if (ImGui::Selectable(mat.Name.c_str()))
                        {
                            *idToChange = id;
                            scene.RequestMeshMaterialUpdate();
                        }
                    }
                    ImGui::EndPopup();
                }

                ImGui::TreePop();
            }
        }

        AddProviderPopup(scene);
        ImGui::EndTabItem();

        if (idToDelete)
        {
            auto it = scene.Meshes.begin() + (*idToDelete);
            scene.Meshes.erase(it);
            scene.RequestGeometryUpdate();
        }
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

    if (ImGui::BeginTabItem("Environment"))
    {
        if (ImGui::Checkbox("Directional light", &scene.Env.DirLightOn))
        {
            scene.RequestEnvironmentUpdate();
        }

        static float phi = 0.125f * 3.14f;
        static float theta = (1.0f - 0.125f) * 3.14f;

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
                scene.GraphRoot.UpdateTransforms(scene.Objects);
                scene.RequestInstanceUpdate();
            }

            ImGui::TreePop();
        }
    }

    ImGui::End();
}

void SceneEditor::SelectHdri()
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