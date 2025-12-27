#pragma once

#include "Event.h"
#include "VulkanContext.h"

struct GLFWwindow;

namespace iminit
{
void InitImGui();

void InitGlfwBackend(GLFWwindow *window);
void InitVulkanBackend(VulkanContext &ctx, uint32_t framesInFlight);

void BeginGuiFrame();
void FinalizeGuiFrame();

struct EventFeedback{
    bool IsBackgroundClicked = false;
};

EventFeedback OnEvent(Event::EventVariant event);

void RecordImguiToCommandBuffer(VkCommandBuffer cmd);

void DestroyImGui(VulkanContext& ctx);
} // namespace iminit