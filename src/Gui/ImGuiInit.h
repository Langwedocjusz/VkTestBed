#pragma once

#include "VulkanContext.h"

struct GLFWwindow;

namespace iminit
{
void InitImGui();
void InitGlfwBackend(GLFWwindow *window);

VkDescriptorPool CreateDescriptorPool(VulkanContext &ctx);

void InitVulkanBackend(VulkanContext &ctx, VkDescriptorPool descriptorPool,
                       uint32_t framesInFlight);

void BeginGuiFrame();
void FinalizeGuiFrame();
void RecordImguiToCommandBuffer(VkCommandBuffer cmd);

void DestroyImGui();
} // namespace iminit