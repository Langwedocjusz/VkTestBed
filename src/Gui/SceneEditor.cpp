#include "SceneEditor.h"

#include "Scene.h"
#include "imgui.h"
#include <ranges>
#include <string>

#include <glm/gtc/type_ptr.hpp>

static bool TransformWidget(InstanceData &data)
{
    auto clampPositive = [](float &x){if (x < 0.0f) x = 0.0f;};

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

void SceneEditor::OnImGui(Scene &scene)
{
    using namespace std::views;

    ImGui::Begin("Scene");

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
        std::string nodeName = "Object " + std::to_string(objId);

        if (ImGui::TreeNodeEx(nodeName.c_str()))
        {
            for (const auto [instId, instance] : enumerate(obj.Instances))
            {
                std::string nodeName = "Instance " + std::to_string(instId);

                if (ImGui::TreeNodeEx(nodeName.c_str()))
                {
                    if (TransformWidget(instance))
                    {
                        scene.UpdateRequested = true;
                        obj.UpdateInstances = true;
                    }

                    ImGui::TreePop();
                }
            }

            auto size = ImVec2(ImGui::GetContentRegionAvail().x, 0.0f);

            if (ImGui::Button("Add instance", size))
            {
                obj.Instances.push_back(InstanceData{});

                scene.UpdateRequested = true;
                obj.UpdateInstances = true;
            }

            ImGui::TreePop();
        }
    }

    ImGui::End();
}