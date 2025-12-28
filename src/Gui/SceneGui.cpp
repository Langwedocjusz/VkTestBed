#include "SceneGui.h"
#include "Pch.h"

#include "ImGuiUtils.h"
#include "ImageData.h"
#include "Keycodes.h"
#include "Scene.h"
#include "Vassert.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <ImGuizmo.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <filesystem>
#include <optional>
#include <ranges>
#include <string>
#include <utility>

static const char *PAYLOAD_STRING = "SCENE_INSTANCE_PAYLOAD";

static ImGuizmo::OPERATION TranslateMode(GizmoMode mode)
{
    switch (mode)
    {
    case GizmoMode::Translate:
        return ImGuizmo::OPERATION::TRANSLATE;
    case GizmoMode::Rotate:
        return ImGuizmo::OPERATION::ROTATE;
    case GizmoMode::Scale:
        return ImGuizmo::OPERATION::SCALE;
    }

    std::unreachable();
}

SceneGui::SceneGui(SceneEditor &editor, const Camera &camera)
    : mEditor(editor), mCamera(camera), mModelLoader(editor)
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

void SceneGui::OnEvent(Event::EventVariant event)
{
    // Change gizmo mode:
    if (mSelectedNode == nullptr)
        return;

    if (!std::holds_alternative<Event::Key>(event))
        return;

    auto e = std::get<Event::Key>(event);

    if (e.Keycode == VKTB_KEY_R && e.Action == VKTB_PRESS)
    {
        if (mGizmoMode == GizmoMode::Rotate)
            mGizmoMode = GizmoMode::Translate;
        else
            mGizmoMode = GizmoMode::Rotate;
    }

    if (e.Keycode == VKTB_KEY_S && e.Action == VKTB_PRESS)
    {
        if (mGizmoMode == GizmoMode::Scale)
            mGizmoMode = GizmoMode::Translate;
        else
            mGizmoMode = GizmoMode::Scale;
    }
}

std::optional<SceneKey> SceneGui::GetSelection() const
{
    if (mSelectedNode)
    {
        if (mSelectedNode->IsLeaf())
        {
            return mSelectedNode->GetObjectKey();
        }
    }

    return std::nullopt;
}

static void GetLeafById(SceneKey id, SceneGraphNode *&result, SceneGraphNode *node)
{
    if (node->IsLeaf())
    {
        if (node->GetObjectKey() == id)
        {
            result = node;
        }
    }
    else
    {
        for (auto &child : node->GetChildren())
        {
            GetLeafById(id, result, child.get());
        }
    }
}

void SceneGui::SetSelection(SceneKey objKey)
{
    if (objKey == 0)
        mSelectedNode = nullptr;
    else
    {
        SceneGraphNode *selectedNode;
        GetLeafById(objKey, selectedNode, &mEditor.GraphRoot);

        if (selectedNode == mSelectedNode)
            mSelectedNode = nullptr;
        else
            mSelectedNode = selectedNode;
    }
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
        vassert(!parent->IsLeaf());
    else
        vassert(!node.IsLeaf());

    // Assemble node name and flags:
    int32_t flags = ImGuiTreeNodeFlags_AllowOverlap;
    flags |= ImGuiTreeNodeFlags_OpenOnDoubleClick;
    flags |= ImGuiTreeNodeFlags_OpenOnArrow;

    if (node.IsLeaf())
        flags |= ImGuiTreeNodeFlags_Leaf;

    if (mSelectedNode == &node)
        flags |= ImGuiTreeNodeFlags_Selected;

    std::string nodeName = node.Name + "##" + std::to_string(childId);

    // Draw the tree node:
    auto nodeState = imutils::TreeNodeExDeleteCopyAble(nodeName, flags);

    // Handle associated drag/drop/clicked events:
    if (nodeState.IsClicked)
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

    if (nodeState.IsDeleted)
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

    if (nodeState.IsCopied)
    {
        auto opData = SceneEditor::NodeOpData{
            .SrcParent = parent,
            .ChildId = childId,
            .DstParent = parent,
        };

        mEditor.ScheduleNodeCopy(opData);
    }

    // Recurse to also draw children nodes:
    if (nodeState.IsOpen)
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

        for (auto &[prefabId, prefab] : mEditor.Prefabs())
        {
            if (!prefab.IsReady)
                continue;

            std::string name = prefab.Root.Name + "##" + std::to_string(prefabId);

            if (ImGui::Selectable(name.c_str()))
            {
                mEditor.InstancePrefab(prefabId);
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

void SceneGui::DataMenu()
{
    ImGui::Begin("Scene data", nullptr, ImGuiWindowFlags_NoFocusOnAppearing);

    auto buttonSize = ImVec2(ImGui::GetContentRegionAvail().x, 0.0f);
    if (ImGui::Button("Full reload", buttonSize))
    {
        mEditor.RequestFullReload();
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

    if (ImGui::BeginTabItem("Images"))
    {
        ImagesTab();
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

    uint32_t counter = 0;

    for (auto &[meshKey, mesh] : mEditor.Meshes())
    {
        std::string nodeName = std::to_string(counter++) + ". " + mesh.Name;

        auto nodeState = imutils::TreeNodeExDeletable(nodeName.c_str());

        if (nodeState.IsDeleted)
        {
            keyToDelete = meshKey;
        }

        if (nodeState.IsOpen)
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
                            auto &originMesh = mEditor.GetMesh((*primToChange).Mesh);
                            auto &prim = originMesh.Primitives[(*primToChange).Idx];

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
        // Just to be safe:
        mSelectedNode = nullptr;

        mEditor.EraseMesh(*keyToDelete);
    }
}

std::string SceneGui::GetMaterialName(std::optional<SceneKey> key,
                                      std::string_view postfix)
{
    if (!key.has_value())
        return std::format("##{}", postfix);

    const auto &imgStr = mEditor.GetImage(*key).Name;

    return std::format("({}) {} ##{}", *key, imgStr, postfix);
}

void SceneGui::MaterialsTab()
{
    auto KeyImageSelection = [this](std::optional<SceneKey> &key,
                                    const std::string &keyName) {
        ImGui::Text("%s", keyName.c_str());
        ImGui::SameLine();

        auto popupName = std::format("Select {}", keyName);
        auto selectableText = GetMaterialName(key, keyName);

        if (ImGui::Selectable(selectableText.c_str()))
        {
            ImGui::OpenPopup(popupName.c_str());
        }

        if (ImGui::BeginPopup(popupName.c_str()))
        {
            static std::array<char, 255> filterBuf{};

            ImGui::InputText("Filter", &filterBuf[0], 255);

            for (auto &[imgPick, _] : mEditor.Images())
            {
                auto imgName =
                    std::format("({}) ", imgPick) + mEditor.GetImage(imgPick).Name;

                if (imgName.find(std::string(&filterBuf[0])) == std::string::npos)
                    continue;

                if (ImGui::Selectable(imgName.c_str()))
                {
                    key = imgPick;
                    mEditor.RequestUpdate(Scene::UpdateFlag::Materials);
                }
            }

            ImGui::EndPopup();
        }
    };

    for (auto &[_, mat] : mEditor.Materials())
    {
        if (ImGui::TreeNodeEx(mat.Name.c_str()))
        {
            KeyImageSelection(mat.Albedo, "Albedo");
            KeyImageSelection(mat.Roughness, "Roughness");
            KeyImageSelection(mat.Normal, "Normal");

            ImGui::Text("Alpha Cutoff: ");
            ImGui::SameLine();

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

            auto lastCutoff = mat.AlphaCutoff;
            ImGui::SliderFloat("##AlphaCutoff", &mat.AlphaCutoff, 0.0f, 1.0f);

            if (mat.AlphaCutoff != lastCutoff)
                mEditor.RequestUpdate(Scene::UpdateFlag::Materials);

            ImGui::TreePop();
        }
    }
}

void SceneGui::ImagesTab()
{
    std::optional<SceneKey> imgToErase = std::nullopt;

    for (auto &[imgKey, img] : mEditor.Images())
    {
        auto nodeState = imutils::TreeNodeExDeletable(img.Name.c_str());

        if (nodeState.IsDeleted)
        {
            imgToErase = imgKey;
        }

        if (nodeState.IsOpen)
        {
            if (img.IsSinglePixel())
            {
                glm::vec4 value = img.GetPixelData();

                const std::string text = "PixelData##Image" + std::to_string(imgKey);

                if (ImGui::ColorEdit4(text.c_str(), glm::value_ptr(value)))
                {
                    img.UpdatePixelData(value);

                    // Images and materials both need to be flagged
                    // since otherwise renderer ends up with
                    // descriptor pointing to non-existent texture.
                    // To-do: think about solving this renderer-side
                    mEditor.RequestUpdate(Scene::UpdateFlag::Images);
                    mEditor.RequestUpdate(Scene::UpdateFlag::Materials);
                }
            }

            ImGui::TreePop();
        }
    }

    if (imgToErase)
    {
        mEditor.EraseImage(*imgToErase);
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

    glm::vec3 newColor = env.LightColor;

    ImGui::ColorEdit3("Color", &newColor.x);

    if (newColor != env.LightColor)
    {
        env.LightColor = newColor;
        mEditor.RequestUpdate(Scene::UpdateFlag::Environment);
    }

    // For debug:
    // ImGui::Text("Current light dir: (%f, %f, %f)", newDir.x, newDir.y, newDir.z);

    ImGui::Text("Hdri path:");

    ImGui::SameLine();

    std::string selText =
        env.HdriImage.has_value() ? env.HdriImage->Name + "##HDRI" : "##HDRI";

    if (ImGui::Selectable(selText.c_str()))
    {
        mOpenHdriPopup = true;
    }

    auto size = ImVec2(ImGui::GetContentRegionAvail().x, 0.0f);

    if (ImGui::Button("Clear hdri", size))
    {
        env.HdriImage = std::nullopt;
        env.ReloadImage = true;

        mEditor.ClearCachedHDRI();
        mEditor.RequestUpdate(Scene::UpdateFlag::Environment);
    }

    if (ImGui::Button("Reload", size))
    {
        env.ReloadImage = true;
        mEditor.RequestUpdate(Scene::UpdateFlag::Environment);
    }

    // Implementation of the popup for hdri selection:
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

        if (mSelectedNode->IsLeaf())
        {
            ImGuiIO &io = ImGui::GetIO();
            ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

            // To-do: currently parent transform is hacked in as part of the
            // camera 'view'. This avoids numerically unstable inversion
            // of the parent matrix at each step of the transformation.
            // The cost is that now all transformations are done in parent
            // coordinate space. By itself this is acceptable, but ImGuizmo
            // is not fully aware, so the widget is sometimes misaligned with
            // the real translation axes etc.

            glm::mat4 currentNonAggregate = mSelectedNode->GetTransform();
            glm::mat4 parentAggregate = mSelectedNode->Parent->GetAggregateTransform();

            auto view = mCamera.GetView() * parentAggregate;
            auto proj = mCamera.GetProj();
            // As usual - Vulkan y orientation:
            proj[1][1] *= -1.0f;

            auto mode = ImGuizmo::MODE::WORLD;
            auto op = TranslateMode(mGizmoMode);

            bool manipulated =
                ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj), op, mode,
                                     glm::value_ptr(currentNonAggregate));

            if (manipulated)
            {
                glm::vec3 scale;
                glm::quat rotation;
                glm::vec3 translation;
                glm::vec3 skew;
                glm::vec4 perspective;
                glm::decompose(currentNonAggregate, scale, rotation, translation, skew,
                               perspective);

                mSelectedNode->Translation = translation;
                mSelectedNode->Rotation = glm::eulerAngles(rotation);
                mSelectedNode->Scale = scale;

                // To-do: optimization, same as above
                mEditor.UpdateTransforms(&mEditor.GraphRoot);
            }
        }
    }

    ImGui::End();
}