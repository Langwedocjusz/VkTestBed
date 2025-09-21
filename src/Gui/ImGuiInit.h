#pragma once

#include "Event.h"
#include "VulkanContext.h"

struct GLFWwindow;

namespace iminit
{
void InitImGui();
void ScaleStyle(float scaleFactor);
void InitGlfwBackend(GLFWwindow *window);

VkDescriptorPool CreateDescriptorPool(VulkanContext &ctx);

void InitVulkanBackend(VulkanContext &ctx, VkDescriptorPool descriptorPool,
                       uint32_t framesInFlight);

void BeginGuiFrame();
void FinalizeGuiFrame();

void RecordImguiToCommandBuffer(VkCommandBuffer cmd);

void ImGuiHandleEvent(Event::EventVariant event);

void DestroyImGui();
} // namespace iminit