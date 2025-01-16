#include "ImGuiUtils.h"

#include "imgui.h"

void imutils::DisplayStats(FrameStats &stats)
{
    // Based on simple overlay from imgui demo
    bool open = true;

    ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

    const int location = 1;
    const float PAD = 10.0f;
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImVec2 work_pos =
        viewport->WorkPos; // Use work area to avoid menu-bar/task-bar, if any!
    ImVec2 work_size = viewport->WorkSize;
    ImVec2 window_pos, window_pos_pivot;
    window_pos.x = (location & 1) ? (work_pos.x + work_size.x - PAD) : (work_pos.x + PAD);
    window_pos.y = (location & 2) ? (work_pos.y + work_size.y - PAD) : (work_pos.y + PAD);
    window_pos_pivot.x = (location & 1) ? 1.0f : 0.0f;
    window_pos_pivot.y = (location & 2) ? 1.0f : 0.0f;
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
    ImGui::SetNextWindowViewport(viewport->ID);

    windowFlags |= ImGuiWindowFlags_NoMove;

    auto bgCol = ImGui::ColorConvertFloat4ToU32(ImVec4(0.06f, 0.06f, 0.06f, 0.33f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, bgCol);

    ImGui::Begin("Engine Stats", &open, windowFlags);

    ImGui::Text("CPU Time: %f [ms]", stats.CPUTime);
    ImGui::Text("GPU Time (Graphics): %f [ms]", stats.GPUTime);
    ImGui::Text("Triangles: %i", stats.NumTriangles);
    ImGui::Text("Draws: %i", stats.NumDraws);
    ImGui::Text("Dispatches: %i", stats.NumDispatches);

    ImGui::End();

    ImGui::PopStyleColor(1);
}