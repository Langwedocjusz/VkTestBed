#include "SceneGui.h"

#include "ImGuiUtils.h"
#include "Scene.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <glm/gtc/type_ptr.hpp>

#include <filesystem>
#include <optional>
#include <ranges>
#include <string>

static const char *PAYLOAD_STRING = "SCENE_INSTANCE_PAYLOAD";

SceneGui::SceneGui(SceneEditor &editor) : mEditor(editor), mModelLoader(editor)
{
    auto path = std::filesystem::current_path() / "assets/cubemaps";

    if (std::filesystem::exists(path))
    {
        mHdriBrowser.CurrentPath = path;
    }

    mHdriBrowser.AddExtensionToFilter(".exr");

    mHdriBrowser.SetCallbackFn([&]() { mEditor.SetHdri(mHdriBrowser.ChosenFile); });

    mHdriBrowser.SetCheckFn([](const std::filesystem::path &path) {
        return std::filesystem::is_regular_file(path);
    });
}

void SceneGui::OnImGui()
{
    DataMenu();
    SceneHierarchyMenu();
    ObjectPropertiesMenu();
}

void SceneGui::SceneHierarchyMenu()
{
    ImGui::Begin("Scene hierarchy", nullptr, ImGuiWindowFlags_NoFocusOnAppearing);

    // Draw nodes for all children of the scene root:
    using namespace std::views;

    for (const auto [id, child] : enumerate(mEditor.GraphRoot.GetChildren()))
        InstanceGui(*child, &mEditor.GraphRoot, id);

    // Setup right-click context menu for adding things:
    AddInstancePopup();

    // Add drop target spanning whole window
    ImGuiWindow *window = ImGui::GetCurrentWindow();

    if (ImGui::BeginDragDropTargetCustom(window->Rect(), window->ID))
    {
        HandleSceneDropPayload(mEditor.GraphRoot);
        ImGui::EndDragDropTarget();
    }

    ImGui::End();

    // Handle model loading if it was scheduled:
    mModelLoader.OnImGui();
}

void SceneGui::InstanceGui(SceneGraphNode &node, SceneGraphNode *parent, int64_t childId)
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

    // Delete button:
    if (imutils::CloseButton(nodeName, closeButtonPos))
    {
        // Reset selection if necessary:
        if (&node == mSelectedNode)
            mSelectedNode = nullptr;

        // Schedule node deletion:
        auto opData = SceneEditor::NodeOpData{
            .SrcParent = parent,
            .ChildId = childId,
            .DstParent = nullptr,
        };

        mEditor.ScheduleNodeDeletion(opData);
    }

    // Copy button:
    {
        ImGui::SameLine();

        // Copy button:
        std::string copyButtonName = "+##" + nodeName;

        ImVec2 plusButtonPos = ImGui::GetCursorScreenPos();
        plusButtonPos.x = closeButtonPos.x - g.Style.FramePadding.x - g.FontSize;
        plusButtonPos.y -= g.Style.FramePadding.y;

        ImGui::SetCursorScreenPos(plusButtonPos);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

        if (ImGui::Button(copyButtonName.c_str()))
        {
            auto opData = SceneEditor::NodeOpData{
                .SrcParent = parent,
                .ChildId = childId,
                .DstParent = parent,
            };

            mEditor.ScheduleNodeCopy(opData);
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

void SceneGui::HandleSceneDropPayload(SceneGraphNode &node)
{
    if (const auto imPayload = ImGui::AcceptDragDropPayload(PAYLOAD_STRING))
    {
        auto payload = static_cast<DragPayload *>(imPayload->Data);

        bool sameNode = &node == payload->Parent;

        bool validTarget = !node.IsLeaf() && !sameNode;

        if (validTarget)
        {
            auto opData = SceneEditor::NodeOpData{
                .SrcParent = payload->Parent,
                .ChildId = payload->ChildId,
                .DstParent = &node,
            };

            mEditor.ScheduleNodeMove(opData);
        }
    }
}

void SceneGui::AddInstancePopup()
{
    using namespace std::views;

    if (ImGui::BeginPopupContextWindow())
    {
        ImGui::Text("Add:");
        ImGui::Separator();

        if (ImGui::Selectable("Empty Group"))
        {
            auto &groupNode = mEditor.GraphRoot.EmplaceChild();
            groupNode.Name = "Group";
        }

        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        ImGui::Text("Instance:");
        ImGui::Separator();

        for (auto [prefabId, prefab] : enumerate(mEditor.Prefabs))
        {
            std::string name = prefab.Name + "##" + std::to_string(prefabId);

            if (ImGui::Selectable(name.c_str()))
            {
                mEditor.InstancePrefab(prefabId);
            }
        }

        /*for (auto &[meshKey, mesh] : mEditor.Meshes())
        {
            std::string name = mesh.Name + "##" + std::to_string(meshKey);

            if (ImGui::Selectable(name.c_str()))
            {
                auto objKey = mEditor.EmplaceObject(meshKey);

                auto &newNode = mEditor.GraphRoot.EmplaceChild(objKey);
                newNode.Name = mesh.Name;

                mEditor.RequestUpdate(Scene::UpdateFlag::Objects);
            }
        }*/

        // To-do: maybe move this to the data menu
        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        if (ImGui::Selectable("Load Model"))
        {
            mModelLoader.TriggerLoad();
        }

        ImGui::EndPopup();
    }
}

void SceneGui::DataMenu()
{
    ImGui::Begin("Scene data", nullptr, ImGuiWindowFlags_NoFocusOnAppearing);

    auto buttonSize = ImVec2(ImGui::GetContentRegionAvail().x, 0.0f);

    if (ImGui::Button("Full reload", buttonSize))
    {
        // To-do: handle this
        // scene.RequestFullUpdate();
    }

    ImGui::BeginTabBar("We");

    if (ImGui::BeginTabItem("Meshes"))
    {
        MeshesTab();
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Materials"))
    {
        MaterialsTab();
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Environment"))
    {
        EnvironmentTab();
        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();

    ImGui::End();
}

void SceneGui::MeshesTab()
{
    using namespace std::views;

    std::optional<SceneKey> keyToDelete = std::nullopt;

    for (auto &[meshKey, mesh] : mEditor.Meshes())
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
            struct PrimId {
                SceneKey Mesh;
                int64_t Idx;
            };

            static std::optional<PrimId> primToChange = std::nullopt;

            for (const auto [primIdx, prim] : enumerate(mesh.Primitives))
            {
                auto id = prim.Material;

                const std::string matName = id ? mEditor.GetMaterial(*id).Name : "None";
                const std::string suffix = "##mat" + mesh.Name + std::to_string(primIdx);

                ImGui::Text("Material %lu: ", primIdx);
                ImGui::SameLine();

                if (ImGui::Selectable((matName + suffix).c_str()))
                {
                    primToChange = {.Mesh = meshKey, .Idx = primIdx};
                    ImGui::OpenPopup("Select material:");
                }
                // To-do: adding new materials
            }

            if (ImGui::BeginPopup("Select material:"))
            {
                if (primToChange)
                {
                    for (auto &[id, mat] : mEditor.Materials())
                    {
                        if (ImGui::Selectable(mat.Name.c_str()))
                        {
                            auto &mesh = mEditor.GetMesh((*primToChange).Mesh);
                            auto &prim = mesh.Primitives[(*primToChange).Idx];

                            prim.Material = id;
                            mEditor.RequestUpdate(Scene::UpdateFlag::MeshMaterials);
                        }
                    }
                }

                ImGui::EndPopup();
            }

            ImGui::TreePop();
        }
    }

    AddProviderPopup();

    if (keyToDelete)
    {
        mEditor.EraseMesh(*keyToDelete);
        // scene.RequestMeshUpdate();
    }
}

void SceneGui::MaterialsTab()
{
    for (auto &[_, mat] : mEditor.Materials())
    {
        if (ImGui::TreeNodeEx(mat.Name.c_str()))
        {
            // To-do: actual menu component
            /*for (const auto &[key, value] : mat)
            {
                std::string entry;
                entry += key.Name;
                entry += ": ";

                // clang-format off
                std::visit(
                    overloaded{
                        [&entry](Material::ImageSource img) {entry += img.Path.string();},
                        [&entry](float val) {entry += std::to_string(val);},
                        [&entry](glm::vec2 v) {entry += std::format("[{}, {}]", v.x,
            v.y);},
                        [&entry](glm::vec3 v) {entry += std::format("[{}, {}, {}]", v.x,
            v.y, v.z);},
                        [&entry](glm::vec4 v) {entry += std::format("[{}, {}, {}, {}]",
            v.x, v.y, v.z, v.w);},
                }, value);
                // clang-format on

                ImGui::Text("%s", entry.c_str());
            }*/

            ImGui::TreePop();
        }
    }
}

void SceneGui::EnvironmentTab()
{
    auto &env = mEditor.GetEnv();

    if (ImGui::Checkbox("Directional light", &env.DirLightOn))
    {
        mEditor.RequestUpdate(Scene::UpdateFlag::Environment);
    }

    static float phi = 2.359f;
    static float theta = 1.650f;

    ImGui::SliderFloat("Azimuth", &phi, 0.0f, 6.28f);
    ImGui::SliderFloat("Altitude", &theta, 0.0f, 3.14f);

    const float cT = cos(theta), sT = sin(theta);
    const float cP = cos(phi), sP = sin(phi);

    glm::vec3 newDir(cP * sT, cT, sP * sT);

    if (newDir != env.LightDir)
    {
        env.LightDir = newDir;
        mEditor.RequestUpdate(Scene::UpdateFlag::Environment);
    }

    ImGui::Text("Hdri path:");

    ImGui::SameLine();

    std::string selText = env.HdriImage ? std::to_string(*env.HdriImage) : "";
    selText += "##HDRI";

    if (ImGui::Selectable(selText.c_str()))
    {
        mOpenHdriPopup = true;
    }

    auto size = ImVec2(ImGui::GetContentRegionAvail().x, 0.0f);

    if (ImGui::Button("Clear hdri", size))
    {
        env.HdriImage = std::nullopt;
        mEditor.RequestUpdate(Scene::UpdateFlag::Environment);
    }

    if (ImGui::Button("Reload", size))
    {
        mEditor.RequestUpdate(Scene::UpdateFlag::Environment);
    }

    // Implementation of the popup for hdri selection:
    SelectHdriPopup();
}

void SceneGui::SelectHdriPopup()
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

void SceneGui::AddProviderPopup()
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

void SceneGui::ObjectPropertiesMenu()
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
                mEditor.UpdateTransforms(&mEditor.GraphRoot);
            }

            ImGui::TreePop();
        }
    }

    ImGui::End();
}