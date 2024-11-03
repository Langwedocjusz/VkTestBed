#include "SceneEditor.h"

#include "Scene.h"
#include "VertexLayout.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <ranges>
#include <string>

#include <glm/gtc/type_ptr.hpp>

static bool TransformWidget(InstanceData &data)
{
    auto clampPositive = [](float &x) {
        if (x < 0.0f)
            x = 0.0f;
    };

    auto prev_trans = data.Translation;
    auto prev_rot = data.Rotation;
    auto prev_scl = data.Scale;

    auto trans_ptr = glm::value_ptr(data.Translation);
    auto rot_ptr = glm::value_ptr(data.Rotation);
    auto scl_ptr = glm::value_ptr(data.Scale);

    const float speed = 0.01f;

    ImGui::DragFloat3("Translation", trans_ptr, speed);
    ImGui::DragFloat3("Rotation", rot_ptr, speed);
    ImGui::DragFloat3("Scale", scl_ptr, speed);

    clampPositive(data.Scale.x);
    clampPositive(data.Scale.y);
    clampPositive(data.Scale.z);

    bool changed = false;
    changed = changed || (prev_trans != data.Translation);
    changed = changed || (prev_rot != data.Rotation);
    changed = changed || (prev_scl != data.Scale);

    return changed;
}

static void DataMenu(Scene &scene)
{
    using namespace std::views;

    auto size = ImVec2(ImGui::GetContentRegionAvail().x, 0.0f);

    if (ImGui::Button("Full reload", size))
    {
        scene.UpdateRequested = true;
        scene.GlobalUpdate = true;

        for (auto &obj : scene.Objects)
            obj.UpdateInstances = true;
    }

    for (const auto [objId, obj] : enumerate(scene.Objects))
    {
        std::string nodeName = obj.Provider.Name + "##" + std::to_string(objId);

        if (ImGui::TreeNodeEx(nodeName.c_str()))
        {
            std::string vertLayout = "Vertex Layout: ";

            for (auto type : obj.Provider.Layout.VertexLayout)
                vertLayout += Vertex::ToString(type) + ", ";

            ImGui::Text("%s", vertLayout.c_str());

            ImGui::TreePop();
        }
    }
}

static void InstanceMenu(Scene &scene)
{
    using namespace std::views;

    struct EraseData{
        //For some reaseon enumerate returns
        //signed index
        int64_t ObjId;
        int64_t InstId;
    };

    std::optional<EraseData> erase;

    for (const auto [objId, obj] : enumerate(scene.Objects))
    {
        for (const auto [instId, instance] : enumerate(obj.Instances))
        {
            std::string nodeName = obj.Provider.Name + " " + std::to_string(instId);
            const auto flags = ImGuiTreeNodeFlags_AllowOverlap;

            const bool nodeOpen = ImGui::TreeNodeEx(nodeName.c_str(), flags);

            ImGui::SameLine();

            ImGuiWindow* window = ImGui::GetCurrentWindow();
            ImGuiID id = window->GetID(nodeName.c_str());
            ImGuiID closeButtonId = ImGui::GetIDWithSeed("#CLOSE", nullptr, id);

            ImGuiContext& g = *GImGui;
            float buttonSize = g.FontSize;
            ImVec2 buttonPos = ImGui::GetCursorScreenPos();
            buttonPos.x += ImGui::GetContentRegionAvail().x - g.Style.FramePadding.x - buttonSize;

            ImGui::NewLine();

            if (ImGui::CloseButton(closeButtonId + 1, buttonPos))
            {
                erase = EraseData{objId, instId};
            }

            if (nodeOpen)
            {

                if (TransformWidget(instance))
                {
                    obj.UpdateInstances = true;
                    scene.UpdateRequested = true;
                }

                ImGui::TreePop();
            }
        }
    }

    if (erase.has_value())
    {
        auto& obj = scene.Objects[erase->ObjId];
        auto& instances = obj.Instances;
        instances.erase(instances.begin() + erase->InstId);

        obj.UpdateInstances = true;
        scene.UpdateRequested = true;
    }
}

static void AddInstanceOnRightClick(Scene &scene)
{
    using namespace std::views;

    if (ImGui::BeginPopupContextWindow())
    {
        ImGui::Text("Add instance:");
        ImGui::Separator();

        for (const auto [objId, obj] : enumerate(scene.Objects))
        {
            std::string name = obj.Provider.Name + "##" + std::to_string(objId);

            if (ImGui::Selectable(name.c_str()))
            {
                obj.Instances.emplace_back();

                obj.UpdateInstances = true;
                scene.UpdateRequested = true;
            }
        }

        ImGui::EndPopup();
    }
}

void SceneEditor::OnImGui(Scene &scene)
{
    ImGui::Begin("Scene");

    ImGui::SameLine();

    ImGui::BeginTabBar("We");

    if (ImGui::BeginTabItem("Instances"))
    {
        InstanceMenu(scene);
        AddInstanceOnRightClick(scene);
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Data"))
    {
        DataMenu(scene);
        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();

    ImGui::End();
}